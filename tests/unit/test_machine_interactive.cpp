#include "fixtures/synthetic_media.hpp"
#include "tests/test_framework.hpp"
#include "xblob/gpu/gpu_types.hpp"
#include "xblob/input/input_types.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/machine/machine_session.hpp"
#include "xblob/usb/ohci_registers.hpp"

#include <vector>

using namespace xblob;
using namespace xblob::machine;
using namespace xblob::input;

TEST_CASE(TestInteractiveInputDuringPauseAndResume) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    // Initially sequence is 0
    EXPECT_EQ(session.input_sequence(), 0ULL);

    // Prepare with synthetic payload to enter Prepared state
    auto xbe = testing::CreateValidSyntheticXbe(0x11110001, "InputTest", 1);
    xbe[0x1000] = 0xF4; // HLT
    MemoryByteSource src(xbe);
    EXPECT_TRUE(session.Prepare(src).has_value());

    // Pause session
    EXPECT_TRUE(session.Pause().has_value());
    EXPECT_TRUE(session.WaitCompletion(100).has_value());

    // Submit input during pause
    HostInputSnapshot s1;
    s1.sequence = 42;
    s1.digital_buttons = buttons::kDPadUp | buttons::kStart;
    s1.button_a = 200;
    s1.thumb_lx = 12000;

    auto sub_res = session.SubmitHostInput(s1);
    EXPECT_TRUE(sub_res.has_value());
    EXPECT_TRUE(*sub_res);
    EXPECT_EQ(session.input_sequence(), 42ULL);

    // Stale snapshot during pause must be rejected and not update sequence
    HostInputSnapshot s_stale;
    s_stale.sequence = 10;
    auto stale_res = session.SubmitHostInput(s_stale);
    EXPECT_TRUE(stale_res.has_value());
    EXPECT_FALSE(*stale_res);
    EXPECT_EQ(session.input_sequence(), 42ULL);

    // Verify gamepad device report reflects snapshot
    auto report = session.gamepad().GetCurrentReport();
    EXPECT_EQ(report.buttons, buttons::kDPadUp | buttons::kStart);
    EXPECT_EQ(report.a, 200u);
    EXPECT_EQ(report.thumb_lx, 12000);

    // Resume session
    EXPECT_TRUE(session.Resume({.max_instructions = 100, .max_cycles = 10000}).has_value());
    EXPECT_TRUE(session.WaitCompletion(200).has_value());
    EXPECT_EQ(session.input_sequence(), 42ULL);
}

TEST_CASE(TestInteractiveCompositeInterrupts) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    auto* intr_source = session.cpu().interrupt_source();
    EXPECT_TRUE(intr_source != nullptr);
    EXPECT_FALSE(intr_source->HasPendingInterrupt());

    // Trigger video interrupt status bit (bit 1: flip complete)
    session.gpu().TriggerVideoInterrupt(0x00000002u);
    EXPECT_FALSE(intr_source->HasPendingInterrupt());

    // Enable video interrupt via MMIO
    EXPECT_TRUE(session.address_space()
                    .Write32(0xFD000000u + gpu::kRegPvideoIntrEn, 0x00000002u)
                    .has_value());
    EXPECT_TRUE(intr_source->HasPendingInterrupt());

    auto ack_gpu = intr_source->AcknowledgeInterrupt();
    EXPECT_TRUE(ack_gpu.has_value());
    EXPECT_EQ(*ack_gpu, 0x23u); // GPU vector

    // Clear GPU interrupt
    session.gpu().AcknowledgeVideoInterrupt(0x00000002u);
    EXPECT_FALSE(intr_source->HasPendingInterrupt());

    // Enable Master Interrupt and RootHubStatusChange in HcInterruptEnable
    EXPECT_TRUE(session.address_space()
                    .Write32(0xFED00000u + usb::ohci::kRegHcInterruptEnable,
                             usb::ohci::interrupt::kMasterInterruptEnable |
                                 usb::ohci::interrupt::kRootHubStatusChange)
                    .has_value());

    // Trigger RootHubStatusChange interrupt in OHCI via port status change
    EXPECT_TRUE(session.ohci().DetachDevice(1).has_value());

    // Now USB IRQ is asserted
    EXPECT_TRUE(intr_source->HasPendingInterrupt());
    auto ack_usb = intr_source->AcknowledgeInterrupt();
    EXPECT_TRUE(ack_usb.has_value());
    EXPECT_EQ(*ack_usb, 0x24u); // USB vector

    // Clear USB interrupt status via MMIO W1C
    EXPECT_TRUE(session.address_space()
                    .Write32(0xFED00000u + usb::ohci::kRegHcInterruptStatus,
                             usb::ohci::interrupt::kRootHubStatusChange)
                    .has_value());
    EXPECT_FALSE(intr_source->HasPendingInterrupt());
}

TEST_CASE(TestInteractiveFrameSequenceAndMetrics) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    auto metrics_init = session.GetInteractiveMetrics();
    EXPECT_EQ(metrics_init.frame_sequence, 0ULL);
    EXPECT_EQ(metrics_init.input_sequence, 0ULL);

    // Flip GPU to advance frame sequence
    session.gpu().Flip();
    EXPECT_EQ(session.gpu().frame_counter(), 1ULL);

    // Step session to process frame sync
    (void)session.Step(1);

    EXPECT_TRUE(session.frame_sequence() >= 0ULL);
}

TEST_CASE(TestAggregatedUnsupportedDiagnostics) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto& session = **session_res;

    // Pushbuffer packet with unsupported method (0x0F00)
    u32 method_offset = 0x0F00;
    u32 header = (1u << 18) | (0u << 13) | method_offset;
    u32 param = 0x12345678;

    std::vector<u32> pb_data = {header, param};
    EXPECT_TRUE(session.address_space().Write32(0x00010000u, pb_data[0]).has_value());
    EXPECT_TRUE(session.address_space().Write32(0x00010004u, pb_data[1]).has_value());

    auto pb_res = session.ExecutePushbuffer(0x00010000u, 2);
    // Method 0x0F00 is unsupported, so execution returns UnsupportedFeature
    EXPECT_FALSE(pb_res.has_value());
    EXPECT_EQ(pb_res.error().code, ErrorCode::UnsupportedFeature);

    // Query aggregated diagnostics
    auto diag = session.GetCompatibilityDiagnostic();
    EXPECT_TRUE(diag.unsupported_features.size() >= 1ULL);

    bool found_gpu = false;
    for (const auto& feat : diag.unsupported_features) {
        if (feat.subsystem == "GPU" && feat.identifier == method_offset) {
            found_gpu = true;
            EXPECT_TRUE(feat.count >= 1ULL);
            EXPECT_FALSE(feat.first_context.empty());
        }
    }
    EXPECT_TRUE(found_gpu);

    // Send unsupported USB setup packet to gamepad
    usb::UsbSetupPacket bad_setup;
    bad_setup.request_type = 0xC0; // Vendor IN
    bad_setup.request = 0xFE;      // Unknown vendor request
    bad_setup.value = 0x1234;
    bad_setup.index = 0x0001;
    bad_setup.length = 0;

    std::vector<u8> dummy_buf(16, 0);
    auto usb_res = session.gamepad().HandleControlTransfer(bad_setup, {}, dummy_buf);
    EXPECT_EQ(usb_res.status, usb::UsbTransferStatus::Stalled);

    // Check interactive metrics has unsupported_usb_count
    auto metrics = session.GetInteractiveMetrics();
    EXPECT_TRUE(metrics.unsupported_usb_count >= 1u);

    // Query aggregated diagnostics again
    auto diag2 = session.GetCompatibilityDiagnostic();
    bool found_usb = false;
    for (const auto& feat : diag2.unsupported_features) {
        if (feat.subsystem == "USB" && feat.identifier == ((0xC0u << 8) | 0xFEu)) {
            found_usb = true;
            EXPECT_TRUE(feat.count >= 1ULL);
            EXPECT_FALSE(feat.first_context.empty());
        }
    }
    EXPECT_TRUE(found_usb);
}

TEST_CASE(TestTwoConsecutiveMachineExecutionsIsolation) {
    // Run 1
    {
        auto s1_res = MachineSession::Create(memory::kRamSizeRetail);
        EXPECT_TRUE(s1_res.has_value());
        auto& s1 = **s1_res;

        auto xbe1 = testing::CreateValidSyntheticXbe(0x11110001, "IsoRun1", 1);
        xbe1[0x1000] = 0xF4; // HLT
        MemoryByteSource src1(xbe1);
        EXPECT_TRUE(s1.Prepare(src1).has_value());

        HostInputSnapshot snap;
        snap.sequence = 100;
        snap.digital_buttons = buttons::kDPadDown;
        EXPECT_TRUE(s1.SubmitHostInput(snap).has_value());
        EXPECT_EQ(s1.input_sequence(), 100ULL);

        EXPECT_TRUE(s1.Start({.max_instructions = 50, .max_cycles = 1000}).has_value());
        EXPECT_TRUE(s1.WaitCompletion(200).has_value());
        EXPECT_TRUE(s1.Stop().has_value());
        EXPECT_TRUE(s1.WaitCompletion(200).has_value());
    }

    // Run 2: Completely independent session, must not inherit state from Run 1
    {
        auto s2_res = MachineSession::Create(memory::kRamSizeRetail);
        EXPECT_TRUE(s2_res.has_value());
        auto& s2 = **s2_res;

        EXPECT_EQ(s2.input_sequence(), 0ULL);
        EXPECT_EQ(s2.frame_sequence(), 0ULL);
        EXPECT_EQ(s2.state(), MachineState::Created);

        auto xbe2 = testing::CreateValidSyntheticXbe(0x22220002, "IsoRun2", 1);
        xbe2[0x1000] = 0xF4; // HLT
        MemoryByteSource src2(xbe2);
        EXPECT_TRUE(s2.Prepare(src2).has_value());

        EXPECT_TRUE(s2.Start({.max_instructions = 50, .max_cycles = 1000}).has_value());
        EXPECT_TRUE(s2.WaitCompletion(200).has_value());
        EXPECT_TRUE(s2.Stop().has_value());
        EXPECT_TRUE(s2.WaitCompletion(200).has_value());
    }
}

int main() {
    return xblob::testing::RunAllTests();
}
