#include "tests/test_framework.hpp"
#include "xblob/gpu/gpu_types.hpp"
#include "xblob/gpu/pushbuffer_types.hpp"
#include "xblob/machine/machine_session.hpp"

using namespace xblob;
using namespace xblob::gpu;
using namespace xblob::machine;

TEST_CASE(TestMachineGpuCompositionAndPci) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    // Verify GPU device is present
    EXPECT_EQ(session.gpu().bdf().ToString(), "00:00.0");
    EXPECT_EQ(session.gpu().config_header().vendor_id(), 0x10DEu);
    EXPECT_EQ(session.gpu().config_header().device_id(), 0x02A0u);

    // Verify BAR0 is active at 0xFD000000
    EXPECT_TRUE(session.gpu().config_header().bar(0).is_active());
    EXPECT_EQ(session.gpu().config_header().bar(0).base_address(), 0xFD000000u);

    // Read PMC Boot0 via guest address space MMIO mapping
    auto boot0_read = session.address_space().Read32(0xFD000000u);
    EXPECT_TRUE(boot0_read.has_value());
    EXPECT_EQ(*boot0_read, kNv2aChipIdRevision);

    // Write PMC Enable via guest address space MMIO
    EXPECT_TRUE(session.address_space().Write32(0xFD000200u, 0x00010001u).has_value());
    auto pmc_en_read = session.address_space().Read32(0xFD000200u);
    EXPECT_TRUE(pmc_en_read.has_value());
    EXPECT_EQ(*pmc_en_read, 0x00010001u);
}

TEST_CASE(TestMachineGpuInterruptIntegration) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    auto* intr_source = session.cpu().interrupt_source();
    EXPECT_TRUE(intr_source != nullptr);

    // Initially no pending interrupt
    EXPECT_FALSE(intr_source->HasPendingInterrupt());

    // Trigger video interrupt status bit (bit 1: flip complete)
    session.gpu().TriggerVideoInterrupt(0x00000002u);

    // Disabled in mask -> no pending interrupt to CPU
    EXPECT_FALSE(intr_source->HasPendingInterrupt());

    // Enable video interrupt in mask via MMIO
    EXPECT_TRUE(
        session.address_space().Write32(0xFD000000u + kRegPvideoIntrEn, 0x00000002u).has_value());

    // Now interrupt is asserted!
    EXPECT_TRUE(intr_source->HasPendingInterrupt());

    // CPU acknowledges interrupt
    auto ack_vec = intr_source->AcknowledgeInterrupt();
    EXPECT_TRUE(ack_vec.has_value());
    EXPECT_EQ(*ack_vec, 0x23u);

    // Acknowledge / clear status via W1C MMIO
    EXPECT_TRUE(
        session.address_space().Write32(0xFD000000u + kRegPvideoIntr, 0x00000002u).has_value());

    // Interrupt line deasserted
    EXPECT_FALSE(intr_source->HasPendingInterrupt());
}

TEST_CASE(TestMachineGpuSessionIsolation) {
    auto session_res_a = MachineSession::Create(memory::kRamSizeRetail);
    auto session_res_b = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res_a.has_value());
    EXPECT_TRUE(session_res_b.has_value());

    auto& session_a = **session_res_a;
    auto& session_b = **session_res_b;

    // Mutate session A's GPU state
    session_a.gpu().back_surface().Clear(255, 0, 128, 255);
    session_a.gpu().Flip();
    EXPECT_EQ(session_a.gpu().frame_counter(), 1u);

    // Session B must be completely untouched
    EXPECT_EQ(session_b.gpu().frame_counter(), 0u);
    EXPECT_TRUE(session_a.gpu().front_surface().data() != session_b.gpu().front_surface().data());
}

TEST_CASE(TestMachineGpuScheduledSubmissionAndCompletion) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    // Write pushbuffer commands to guest RAM:
    // 1. Clear with solid green
    // 2. Draw red rect
    // 3. Flip
    const GuestAddr pb_addr = 0x00010000u;
    const std::vector<u32> pb_words = {
        (2u << 18) | kMethodClearColor,
        0x0000FF00u, // RGBA green
        1u,          // Clear trigger
        (6u << 18) | kMethodRectX,
        20u,
        20u,
        40u,
        40u,
        0xFF0000FFu, // RGBA red
        1u,          // Draw trigger
        (1u << 18) | kMethodFlip,
        1u,
    };

    for (std::size_t i = 0; i < pb_words.size(); ++i) {
        EXPECT_TRUE(session.address_space()
                        .Write32(static_cast<GuestAddr>(pb_addr + i * 4), pb_words[i])
                        .has_value());
    }

    // Schedule submission with 50 cycles delay
    auto event_id_res = session.SchedulePushbuffer(pb_addr, static_cast<u32>(pb_words.size()), 50);
    EXPECT_TRUE(event_id_res.has_value());

    // Cycle 0: front surface has sequence 0
    EXPECT_EQ(session.GetLatestFrameMetadata().sequence_number, 0u);

    // Step 40 cycles: event not reached
    EXPECT_TRUE(session.scheduler().StepCycles(40).has_value());
    EXPECT_EQ(session.GetLatestFrameMetadata().sequence_number, 0u);

    // Step to cycle 55 (event fires, completes pushbuffer, schedules flip completion)
    EXPECT_TRUE(session.scheduler().StepCycles(15).has_value());

    // Step past completion cycle (+200 cycles)
    EXPECT_TRUE(session.scheduler().StepCycles(250).has_value());

    // Frame flip completed!
    EXPECT_EQ(session.GetLatestFrameMetadata().sequence_number, 1u);
    EXPECT_EQ(*session.gpu().front_surface().GetPixel(25, 25), 0xFF0000FFu);
    EXPECT_EQ(*session.gpu().front_surface().GetPixel(0, 0), 0x0000FF00u);
}

TEST_CASE(TestMachineGpuEndToEndWorkloadDeterminism) {
    auto run_workload = []() -> std::vector<u8> {
        auto session_res = MachineSession::Create(memory::kRamSizeRetail);
        auto& session = **session_res;

        const GuestAddr pb_addr = 0x00020000u;
        const std::vector<u32> pb_words = {
            (2u << 18) | kMethodClearColor,
            0x00001020u,
            1u,
            (6u << 18) | kMethodRectX,
            15u,
            15u,
            50u,
            50u,
            0xCCDDEEFFu,
            1u,
            (1u << 18) | kMethodFlip,
            1u,
        };

        for (std::size_t i = 0; i < pb_words.size(); ++i) {
            (void)session.address_space().Write32(static_cast<GuestAddr>(pb_addr + i * 4),
                                                  pb_words[i]);
        }

        (void)session.ExecutePushbuffer(pb_addr, static_cast<u32>(pb_words.size()));
        return session.gpu().front_surface().data();
    };

    auto frame1 = run_workload();
    auto frame2 = run_workload();

    // Verify 100% byte-for-byte reproducibility
    EXPECT_EQ(frame1.size(), frame2.size());
    EXPECT_TRUE(frame1 == frame2);
}

int main() {
    return xblob::testing::RunAllTests();
}
