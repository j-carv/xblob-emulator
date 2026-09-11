#include "tests/test_framework.hpp"
#include "xblob/bus/bus.hpp"
#include "xblob/gpu/gpu_register_file.hpp"
#include "xblob/gpu/gpu_surface.hpp"
#include "xblob/gpu/gpu_types.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/pushbuffer_processor.hpp"

using namespace xblob;
using namespace xblob::gpu;

TEST_CASE(TestGpuRegistersAllowlistAndMasks) {
    auto dev = Nv2aDevice::Create();

    // 1. Known register: PMC Boot 0 is read-only chip ID
    auto boot0_res = dev->Read(kRegPmcBoot0, bus::BusAccessWidth::Dword);
    EXPECT_TRUE(boot0_res.has_value());
    EXPECT_EQ(*boot0_res, kNv2aChipIdRevision);

    // Write to read-only register shouldn't change value
    EXPECT_TRUE(dev->Write(kRegPmcBoot0, bus::BusAccessWidth::Dword, 0x12345678).has_value());
    EXPECT_EQ(*dev->Read(kRegPmcBoot0, bus::BusAccessWidth::Dword), kNv2aChipIdRevision);

    // 2. Read/Write mask: PMC Enable
    EXPECT_TRUE(dev->Write(kRegPmcEnable, bus::BusAccessWidth::Dword, 0xFFFFFFFF).has_value());
    // Only bits 0, 12, 16 are writable (0x00011001)
    EXPECT_EQ(*dev->Read(kRegPmcEnable, bus::BusAccessWidth::Dword), 0x00011001u);

    // 3. W1C behavior on PVIDEO Interrupt
    dev->TriggerVideoInterrupt(0x00000003);
    EXPECT_EQ(*dev->Read(kRegPvideoIntr, bus::BusAccessWidth::Dword), 0x00000003u);
    // Write 1 to bit 0 to clear it
    EXPECT_TRUE(dev->Write(kRegPvideoIntr, bus::BusAccessWidth::Dword, 0x00000001).has_value());
    EXPECT_EQ(*dev->Read(kRegPvideoIntr, bus::BusAccessWidth::Dword), 0x00000002u);

    // 4. Misaligned access check
    auto misaligned_res = dev->Read(0x00000002, bus::BusAccessWidth::Dword);
    EXPECT_FALSE(misaligned_res.has_value());
    EXPECT_EQ(dev->fault(), GpuFault::MisalignedRegisterAccess);

    // 5. Unknown register check
    auto unknown_res = dev->Read(0x0000AB00, bus::BusAccessWidth::Dword);
    EXPECT_FALSE(unknown_res.has_value());
    EXPECT_EQ(dev->fault(), GpuFault::UnknownRegister);
}

TEST_CASE(TestGpuSurfaceHostileDimensionsAndGoldenPixels) {
    // Zero dimensions
    EXPECT_FALSE(GpuSurface::Create(0, 480).has_value());
    EXPECT_FALSE(GpuSurface::Create(640, 0).has_value());

    // Exceeding maximum allowed resolution
    EXPECT_FALSE(GpuSurface::Create(4096, 2160).has_value());

    // Hostile pitch smaller than width * 4
    EXPECT_FALSE(GpuSurface::Create(640, 480, 100).has_value());

    // Valid surface
    auto surf_res = GpuSurface::Create(64, 64);
    EXPECT_TRUE(surf_res.has_value());
    auto surf = std::move(*surf_res);

    EXPECT_EQ(surf.width(), 64u);
    EXPECT_EQ(surf.height(), 64u);
    EXPECT_EQ(surf.pitch(), 256u);
    EXPECT_EQ(surf.byte_size(), 64u * 256u);

    // Clear surface with solid blue (RGBA: 0, 0, 255, 255)
    surf.Clear(0, 0, 255, 255);
    EXPECT_EQ(*surf.GetPixel(0, 0),
              0xFFFF0000u); // Little-endian 0xRRGGBBAA -> AA BB GG RR: 0xFF, 0xFF, 0x00, 0x00
    EXPECT_EQ(*surf.GetPixel(32, 32), 0xFFFF0000u);

    // Draw red rectangle at (10, 10) size (20, 20) (RGBA: 255, 0, 0, 255)
    EXPECT_TRUE(surf.FillRect(10, 10, 20, 20, 255, 0, 0, 255).has_value());

    // Inside rectangle: red pixel
    EXPECT_EQ(*surf.GetPixel(10, 10), 0xFF0000FFu);
    EXPECT_EQ(*surf.GetPixel(29, 29), 0xFF0000FFu);

    // Outside rectangle: blue pixel
    EXPECT_EQ(*surf.GetPixel(9, 10), 0xFFFF0000u);
    EXPECT_EQ(*surf.GetPixel(30, 30), 0xFFFF0000u);

    // Hostile bounds check: out of surface bounds
    auto out_of_bounds = surf.FillRect(50, 50, 30, 30, 0, 255, 0, 255);
    EXPECT_FALSE(out_of_bounds.has_value());
    EXPECT_EQ(out_of_bounds.error().code, ErrorCode::OutOfBounds);
    // Ensure no partial writes occurred
    EXPECT_EQ(*surf.GetPixel(50, 50), 0xFFFF0000u);
}

TEST_CASE(TestGpuResetAndDeterministicLifecycle) {
    auto dev1 = Nv2aDevice::Create();

    // Dirty dev1 state
    dev1->register_file().SetValue(kRegPvideoBuffer, 0x03000000);
    dev1->TriggerVideoInterrupt(0x00000003);
    dev1->back_surface().Clear(128, 64, 32, 255);
    dev1->Flip();
    dev1->SetFault(GpuFault::UnknownMethod);
    EXPECT_EQ(dev1->frame_counter(), 1u);
    EXPECT_NE(dev1->fault(), GpuFault::None);

    // Reset dev1
    dev1->Reset();

    // Create fresh instance
    auto dev2 = Nv2aDevice::Create();

    // Verify byte-for-byte match
    EXPECT_EQ(dev1->fault(), dev2->fault());
    EXPECT_EQ(dev1->frame_counter(), dev2->frame_counter());
    EXPECT_EQ(dev1->pending_events().size(), dev2->pending_events().size());
    EXPECT_TRUE(dev1->front_surface().data() == dev2->front_surface().data());
    EXPECT_TRUE(dev1->back_surface().data() == dev2->back_surface().data());

    // Verify registers match
    EXPECT_EQ(dev1->register_file().GetValue(kRegPmcBoot0),
              dev2->register_file().GetValue(kRegPmcBoot0));
    EXPECT_EQ(dev1->register_file().GetValue(kRegPvideoBuffer),
              dev2->register_file().GetValue(kRegPvideoBuffer));
    EXPECT_EQ(dev1->register_file().GetValue(kRegPvideoIntr),
              dev2->register_file().GetValue(kRegPvideoIntr));
}

TEST_CASE(TestPushbufferDecoderDefensive) {
    // 1. Valid incrementing packet: ClearColor (0x0120) with 2 parameters
    const std::vector<u32> valid_stream = {
        // Method 0x0120, count = 2, incrementing
        (2u << 18) | 0x0120u,
        0xAABBCCDDu,
        0x00000001u,
    };

    std::size_t offset = 0;
    auto pkt_res = PushbufferDecoder::DecodeFromSpan(valid_stream, offset);
    EXPECT_TRUE(pkt_res.has_value());
    EXPECT_EQ(pkt_res->opcode, PacketOpcode::Method);
    EXPECT_EQ(pkt_res->method, 0x0120u);
    EXPECT_EQ(pkt_res->count, 2u);
    EXPECT_FALSE(pkt_res->non_incrementing);
    EXPECT_EQ(pkt_res->parameters.size(), 2u);
    EXPECT_EQ(pkt_res->parameters[0], 0xAABBCCDDu);
    EXPECT_EQ(pkt_res->parameters[1], 0x00000001u);

    // 2. Truncated packet
    const std::vector<u32> truncated_stream = {
        // Count = 4, but only 1 parameter following
        (4u << 18) | 0x0140u,
        0x00000010u,
    };

    offset = 0;
    auto trunc_res = PushbufferDecoder::DecodeFromSpan(truncated_stream, offset);
    EXPECT_FALSE(trunc_res.has_value());
    EXPECT_EQ(trunc_res.error().code, ErrorCode::TruncatedData);
}

TEST_CASE(TestPushbufferExecutionAndAtomicCommit) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor processor(*dev);

    // Build pushbuffer stream:
    // 1. Set clear color to 0x0000FF00 (green) and clear back surface
    // 2. Set rect (10, 10, 20, 20) color 0xFF0000FF (red) and draw
    // 3. Flip
    const std::vector<u32> commands = {
        // Method 0x0120 (ClearColor), count = 2
        (2u << 18) | kMethodClearColor,
        0x0000FF00u, // RGBA green
        1u,          // Clear trigger

        // Method 0x0140 (RectX), count = 6, incrementing
        (6u << 18) | kMethodRectX,
        10u,         // x
        10u,         // y
        20u,         // w
        20u,         // h
        0xFF0000FFu, // RGBA red
        1u,          // Draw trigger

        // Flip
        (1u << 18) | kMethodFlip,
        1u,
    };

    auto stats_res = processor.ExecuteBuffer(commands);
    EXPECT_TRUE(stats_res.has_value());
    EXPECT_EQ(stats_res->packets_processed, 3u);
    EXPECT_EQ(dev->frame_counter(), 1u);

    // Check golden pixels on front surface
    const auto& front = dev->front_surface();
    // Inside rect: red (0xFF0000FF)
    EXPECT_EQ(*front.GetPixel(15, 15), 0xFF0000FFu);
    // Outside rect: green (0x0000FF00)
    EXPECT_EQ(*front.GetPixel(0, 0), 0x0000FF00u);

    // Atomic commit on fault test:
    // Send packet with invalid rect coordinates (width 1000 at x = 100 on 640 surface)
    const std::vector<u32> invalid_commands = {
        (6u << 18) | kMethodRectX,
        100u,
        100u,
        1000u, // Exceeds 640!
        50u,
        0x12345678u,
        1u,
    };

    auto invalid_res = processor.ExecuteBuffer(invalid_commands);
    EXPECT_FALSE(invalid_res.has_value());
    EXPECT_EQ(dev->fault(), GpuFault::InvalidCoordinates);
    // Ensure back surface remains unchanged
    EXPECT_EQ(*dev->back_surface().GetPixel(100, 100), 0x0000FF00u);
}

TEST_CASE(TestPushbufferBudgetsAndLoopProtection) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor processor(*dev);

    // Pushbuffer with cyclic jump to word index 0
    const std::vector<u32> cyclic_stream = {
        0x00000001u, // Jump to 0x00000000 (itself)
    };

    PushbufferBudgets budgets;
    budgets.max_jumps = 5;

    auto loop_res = processor.ExecuteBuffer(cyclic_stream, budgets);
    EXPECT_FALSE(loop_res.has_value());
    EXPECT_EQ(loop_res.error().code, ErrorCode::LimitReached);
    EXPECT_EQ(dev->fault(), GpuFault::BudgetExhausted);
}

int main() {
    return xblob::testing::RunAllTests();
}
