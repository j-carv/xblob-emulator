#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/machine/machine_session.hpp"
#include "xblob/machine/media_boot_pipeline.hpp"

using namespace xblob;
using namespace xblob::machine;

TEST_CASE(TestMachineSessionLifecycleTransitions) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Initial state is Created
    EXPECT_EQ(session->state(), MachineState::Created);

    // Step or Run on un-prepared session must fail with InvalidState
    auto step_err = session->Step(1);
    EXPECT_FALSE(step_err.has_value());
    EXPECT_EQ(step_err.error().code, ErrorCode::InvalidState);

    auto run_err = session->RunWithBudget(10, 100);
    EXPECT_FALSE(run_err.has_value());
    EXPECT_EQ(run_err.error().code, ErrorCode::InvalidState);

    // Prepare valid synthetic XBE
    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
    // Write HLT at entry point (0x1000 in raw file)
    xbe_bytes[0x1000] = 0xF4; // HLT
    MemoryByteSource source(xbe_bytes);

    auto prep_res = session->Prepare(source);
    EXPECT_TRUE(prep_res.has_value());
    EXPECT_EQ(session->state(), MachineState::Prepared);

    // Preparing again must fail with InvalidState without altering state
    auto prep2_res = session->Prepare(source);
    EXPECT_FALSE(prep2_res.has_value());
    EXPECT_EQ(prep2_res.error().code, ErrorCode::InvalidState);
    EXPECT_EQ(session->state(), MachineState::Prepared);

    // Pause transitions Prepared -> Paused
    EXPECT_TRUE(session->Pause().has_value());
    EXPECT_EQ(session->state(), MachineState::Paused);

    // Step executing HLT
    auto step_hlt = session->Step(1);
    EXPECT_TRUE(step_hlt.has_value());
    EXPECT_EQ(step_hlt->instructions_executed, 1u);
    EXPECT_EQ(step_hlt->cpu_result, cpu::StepResult::Halted);
    EXPECT_EQ(session->state(), MachineState::Paused);

    // Stop session -> Stopped
    EXPECT_TRUE(session->Stop().has_value());
    EXPECT_EQ(session->state(), MachineState::Stopped);

    // Step after stop fails
    auto step_stop = session->Step(1);
    EXPECT_FALSE(step_stop.has_value());
    EXPECT_EQ(step_stop.error().code, ErrorCode::InvalidState);

    // Pause after stop fails
    auto pause_stop = session->Pause();
    EXPECT_FALSE(pause_stop.has_value());
    EXPECT_EQ(pause_stop.error().code, ErrorCode::InvalidState);
}

TEST_CASE(TestMachineSessionIsolation) {
    auto s1_res = MachineSession::Create(memory::kRamSizeRetail);
    auto s2_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(s1_res.has_value());
    EXPECT_TRUE(s2_res.has_value());
    auto s1 = std::move(*s1_res);
    auto s2 = std::move(*s2_res);

    // Session 1: MOV EAX, 0x11111111; HLT
    auto xbe1 = testing::CreateValidSyntheticXbe(0x11111111, "App 1", 1);
    xbe1[0x1000] = 0xB8;
    xbe1[0x1001] = 0x11;
    xbe1[0x1002] = 0x11;
    xbe1[0x1003] = 0x11;
    xbe1[0x1004] = 0x11;
    xbe1[0x1005] = 0xF4;
    MemoryByteSource src1(xbe1);
    EXPECT_TRUE(s1->Prepare(src1).has_value());

    // Session 2: MOV EAX, 0x22222222; HLT
    auto xbe2 = testing::CreateValidSyntheticXbe(0x22222222, "App 2", 1);
    xbe2[0x1000] = 0xB8;
    xbe2[0x1001] = 0x22;
    xbe2[0x1002] = 0x22;
    xbe2[0x1003] = 0x22;
    xbe2[0x1004] = 0x22;
    xbe2[0x1005] = 0xF4;
    MemoryByteSource src2(xbe2);
    EXPECT_TRUE(s2->Prepare(src2).has_value());

    // Step s1 only
    auto run1 = s1->RunWithBudget(10, 100);
    EXPECT_TRUE(run1.has_value());
    EXPECT_EQ(s1->cpu().context().GetGpr(cpu::Reg32::EAX), 0x11111111u);

    // Ensure s2 was not mutated
    EXPECT_EQ(s2->state(), MachineState::Prepared);
    EXPECT_EQ(s2->cpu().context().GetGpr(cpu::Reg32::EAX), 0u);
    EXPECT_EQ(s2->scheduler().current_cycle(), 0u);

    // Step s2
    auto run2 = s2->RunWithBudget(10, 100);
    EXPECT_TRUE(run2.has_value());
    EXPECT_EQ(s2->cpu().context().GetGpr(cpu::Reg32::EAX), 0x22222222u);

    // Verify independent RAM values
    auto r1_word = s1->address_space().Read32(0x00011001);
    auto r2_word = s2->address_space().Read32(0x00011001);
    EXPECT_TRUE(r1_word.has_value());
    EXPECT_TRUE(r2_word.has_value());
    EXPECT_EQ(*r1_word, 0x11111111u);
    EXPECT_EQ(*r2_word, 0x22222222u);
}

TEST_CASE(TestMachineSessionDeterminismAcrossRuns) {
    auto make_run = []() {
        auto s_res = MachineSession::Create(memory::kRamSizeRetail);
        auto s = std::move(s_res.value());
        auto xbe = testing::CreateValidSyntheticXbe(0x9999, "DetTest", 1);
        // MOV EAX, 10
        // ADD EAX, 5
        // HLT
        xbe[0x1000] = 0xB8;
        xbe[0x1001] = 0x0A;
        xbe[0x1002] = 0x00;
        xbe[0x1003] = 0x00;
        xbe[0x1004] = 0x00;
        xbe[0x1005] = 0x05;
        xbe[0x1006] = 0x05;
        xbe[0x1007] = 0x00;
        xbe[0x1008] = 0x00;
        xbe[0x1009] = 0x00;
        xbe[0x100A] = 0xF4;
        MemoryByteSource src(xbe);
        (void)s->Prepare(src);
        auto outcome = s->RunWithBudget(100, 1000);
        return std::make_tuple(s->cpu().context(), s->scheduler().current_cycle(), outcome.value());
    };

    auto [ctx1, cycle1, out1] = make_run();
    auto [ctx2, cycle2, out2] = make_run();

    EXPECT_EQ(ctx1.eip, ctx2.eip);
    EXPECT_EQ(ctx1.GetGpr(cpu::Reg32::EAX), 15u);
    EXPECT_EQ(ctx1.GetGpr(cpu::Reg32::EAX), ctx2.GetGpr(cpu::Reg32::EAX));
    EXPECT_EQ(cycle1, cycle2);
    EXPECT_EQ(out1.instructions_executed, out2.instructions_executed);
    EXPECT_EQ(out1.cycles_consumed, out2.cycles_consumed);
}

TEST_CASE(TestMediaBootPipelineDirectXbe) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x11112222, "Direct XBE Test", 1);
    xbe_bytes[0x1000] = 0xF4; // HLT
    auto source = std::make_shared<MemoryByteSource>(xbe_bytes);

    auto prep_res = session->PrepareMedia(source);
    EXPECT_TRUE(prep_res.has_value());
    EXPECT_EQ(session->state(), MachineState::Prepared);
    EXPECT_TRUE(session->media_source() != nullptr);
    EXPECT_TRUE(session->vfs() != nullptr);

    // Step session executing HLT
    auto step_res = session->Step(1);
    EXPECT_TRUE(step_res.has_value());
    EXPECT_EQ(step_res->cpu_result, cpu::StepResult::Halted);
}

TEST_CASE(TestMediaBootPipelineXdvdfsTrimmedIso) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x33334444, "Disc Game", 1);
    xbe_bytes[0x1000] = 0xF4; // HLT

    auto img = testing::BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", xbe_bytes},
        {"SYSTEM/GAME.CFG", {'1', '2', '3'}},
    });
    auto source = std::make_shared<MemoryByteSource>(std::move(img));

    auto prep_res = session->PrepareMedia(source);
    EXPECT_TRUE(prep_res.has_value());
    EXPECT_EQ(session->state(), MachineState::Prepared);
    EXPECT_TRUE(session->media_source() != nullptr);
    EXPECT_TRUE(session->vfs() != nullptr);
    EXPECT_TRUE(session->vfs()->IsMounted("D"));

    // Query file on disc through session's VFS
    auto query_res = session->vfs()->QueryPath("D:\\system\\game.cfg");
    EXPECT_TRUE(query_res.has_value());
    EXPECT_EQ(query_res->size, 3ULL);

    // Step session executing HLT
    auto step_res = session->Step(1);
    EXPECT_TRUE(step_res.has_value());
    EXPECT_EQ(step_res->cpu_result, cpu::StepResult::Halted);
}

TEST_CASE(TestMediaBootPipelineNonXdvdfsIsoRejectionRollback) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Standard non-XDVDFS dummy source
    std::vector<u8> dummy_iso(64 * 2048, 0x00);
    auto source = std::make_shared<MemoryByteSource>(dummy_iso);

    auto prep_res = session->PrepareMedia(source);
    EXPECT_FALSE(prep_res.has_value());
    EXPECT_EQ(prep_res.error().code, ErrorCode::UnknownFormat);
    // Session state MUST remain Created (no leaked state)
    EXPECT_EQ(session->state(), MachineState::Created);
}

TEST_CASE(TestMediaBootPipelineMissingDefaultXbeRejectionRollback) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Valid XDVDFS disc image, but missing default.xbe
    auto img = testing::BuildValidTrimmedXdvdfsImage({
        {"OTHER.XBE", {'X', 'B', 'E', '1'}},
    });
    auto source = std::make_shared<MemoryByteSource>(std::move(img));

    auto prep_res = session->PrepareMedia(source);
    EXPECT_FALSE(prep_res.has_value());
    EXPECT_EQ(prep_res.error().code, ErrorCode::FileNotFound);
    // Session state MUST remain Created
    EXPECT_EQ(session->state(), MachineState::Created);
}

TEST_CASE(TestMediaBootPipelineRawRedumpIso) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x99990000, "Redump Disc", 1);
    xbe_bytes[0x1000] = 0xF4; // HLT

    auto source = testing::CreateSyntheticRawXdvdfsSource({
        {"DEFAULT.XBE", xbe_bytes},
    });

    auto prep_res = session->PrepareMedia(source);
    EXPECT_TRUE(prep_res.has_value());
    EXPECT_EQ(session->state(), MachineState::Prepared);
    EXPECT_TRUE(session->vfs() != nullptr);
    EXPECT_TRUE(session->vfs()->IsMounted("D"));

    auto step_res = session->Step(1);
    EXPECT_TRUE(step_res.has_value());
    EXPECT_EQ(step_res->cpu_result, cpu::StepResult::Halted);
}

TEST_CASE(TestMediaBootPipelineCorruptedXbeRollback) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Valid magic XBEH but truncated/corrupt header
    std::vector<u8> corrupt_xbe = {'X', 'B', 'E', 'H', 0x00, 0x00, 0x00, 0x00};
    auto source = std::make_shared<MemoryByteSource>(std::move(corrupt_xbe));

    auto prep_res = session->PrepareMedia(source);
    EXPECT_FALSE(prep_res.has_value());
    // Session state MUST remain Created
    EXPECT_EQ(session->state(), MachineState::Created);
}

TEST_CASE(TestMachineSessionAsyncWorkerLifecycle) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Program: ADD EAX, 1; HLT
    auto xbe = testing::CreateValidSyntheticXbe(0x44440001, "Async Test", 1);
    xbe[0x1000] = 0x83;
    xbe[0x1001] = 0xC0;
    xbe[0x1002] = 0x01; // ADD EAX, 1
    xbe[0x1003] = 0xF4; // HLT
    MemoryByteSource src(xbe);
    EXPECT_TRUE(session->Prepare(src).has_value());

    ExecutionBudgets budgets{};
    budgets.max_instructions = 100;
    budgets.max_cycles = 1000;
    budgets.chunk_instructions = 10;

    EXPECT_TRUE(session->Start(budgets).has_value());
    EXPECT_TRUE(session->WaitCompletion(2000).value_or(false));
    EXPECT_EQ(session->state(), MachineState::Paused);

    auto snap = session->GetSnapshot();
    EXPECT_EQ(snap.last_stop_reason.code, StopReasonCode::Halted);
    EXPECT_EQ(snap.cpu_context.GetGpr(cpu::Reg32::EAX), 1u);
    EXPECT_EQ(snap.instructions_executed, 2u);

    EXPECT_TRUE(session->Stop().has_value());
    EXPECT_EQ(session->state(), MachineState::Stopped);
}

TEST_CASE(TestMachineSessionWatchdogAndWallTimeBudget) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Infinite loop: 0xEB, 0xFE (JMP $)
    auto xbe = testing::CreateValidSyntheticXbe(0x44440002, "Watchdog Test", 1);
    xbe[0x1000] = 0xEB;
    xbe[0x1001] = 0xFE;
    MemoryByteSource src(xbe);
    EXPECT_TRUE(session->Prepare(src).has_value());

    ExecutionBudgets budgets{};
    budgets.max_wall_time_ms = 40; // 40ms watchdog limit
    budgets.chunk_instructions = 50;

    EXPECT_TRUE(session->Start(budgets).has_value());
    EXPECT_TRUE(session->WaitCompletion(3000).value_or(false));
    EXPECT_EQ(session->state(), MachineState::Paused);

    auto snap = session->GetSnapshot();
    EXPECT_EQ(snap.last_stop_reason.code, StopReasonCode::BudgetWallTimeExhausted);
    EXPECT_TRUE(snap.instructions_executed > 0);

    EXPECT_TRUE(session->Stop().has_value());
}

TEST_CASE(TestMachineSessionStructuredStopReasonUnsupportedExport) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    // Program: MOV EAX, 0x8888; INT 0x2D; RET
    auto xbe = testing::CreateValidSyntheticXbe(0x44440003, "Export Test", 1);
    xbe[0x1000] = 0xB8;
    xbe[0x1001] = 0x88;
    xbe[0x1002] = 0x88;
    xbe[0x1003] = 0x00;
    xbe[0x1004] = 0x00; // MOV EAX, 0x8888
    xbe[0x1005] = 0xCD;
    xbe[0x1006] = 0x2D; // INT 0x2D
    xbe[0x1007] = 0xC3; // RET
    MemoryByteSource src(xbe);
    EXPECT_TRUE(session->Prepare(src).has_value());

    auto step_res = session->Step(2);
    EXPECT_TRUE(step_res.has_value());
    EXPECT_EQ(session->state(), MachineState::Faulted);

    auto snap = session->GetSnapshot();
    EXPECT_EQ(snap.last_stop_reason.code, StopReasonCode::UnsupportedExport);
    EXPECT_EQ(snap.last_stop_reason.ordinal_or_opcode, 0x8888u);
    EXPECT_EQ(snap.last_stop_reason.category, "Kernel");

    auto diag = session->GetCompatibilityDiagnostic();
    EXPECT_EQ(diag.first_blocker.code, StopReasonCode::UnsupportedExport);
    EXPECT_EQ(diag.first_blocker.ordinal_or_opcode, 0x8888u);
}

TEST_CASE(TestMachineSessionSnapshotStackInspection) {
    auto session_res = MachineSession::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(session_res.has_value());
    auto session = std::move(*session_res);

    auto xbe = testing::CreateValidSyntheticXbe(0x44440004, "Stack Test", 1);
    xbe[0x1000] = 0xF4; // HLT
    MemoryByteSource src(xbe);
    EXPECT_TRUE(session->Prepare(src).has_value());

    // 1. ESP in valid RAM: write pattern to stack
    session->cpu().context().SetGpr(cpu::Reg32::ESP, 0x00040000);
    EXPECT_TRUE(session->address_space().Write32(0x00040000, 0x11223344).has_value());
    EXPECT_TRUE(session->address_space().Write32(0x00040004, 0x55667788).has_value());

    auto snap1 = session->GetSnapshot();
    EXPECT_TRUE(snap1.stack_valid);
    EXPECT_TRUE(snap1.stack_words.size() >= 2u);
    EXPECT_EQ(snap1.stack_words[0], 0x11223344u);
    EXPECT_EQ(snap1.stack_words[1], 0x55667788u);

    // 2. ESP in unmapped address: must not crash and mark stack invalid
    session->cpu().context().SetGpr(cpu::Reg32::ESP, 0xD0000000);
    auto snap2 = session->GetSnapshot();
    EXPECT_FALSE(snap2.stack_valid);
    EXPECT_TRUE(snap2.stack_words.empty());
}

TEST_CASE(TestMachineSessionDeterministicRunsAcrossSessions) {
    auto run_once = []() {
        auto s_res = MachineSession::Create(memory::kRamSizeRetail);
        auto s = std::move(*s_res);
        auto xbe = testing::CreateValidSyntheticXbe(0x55550001, "Det Test", 1);
        // Code: ADD EAX, 5; ADD EBX, 10; HLT
        xbe[0x1000] = 0x83;
        xbe[0x1001] = 0xC0;
        xbe[0x1002] = 0x05; // ADD EAX, 5
        xbe[0x1003] = 0x83;
        xbe[0x1004] = 0xC3;
        xbe[0x1005] = 0x0A; // ADD EBX, 10
        xbe[0x1006] = 0xF4; // HLT
        MemoryByteSource src(xbe);
        (void)s->Prepare(src);
        ExecutionBudgets b{};
        b.max_instructions = 100;
        b.max_cycles = 1000;
        (void)s->Start(b);
        (void)s->WaitCompletion(2000);
        return s->GetSnapshot();
    };

    auto snap1 = run_once();
    auto snap2 = run_once();

    EXPECT_EQ(snap1.instructions_executed, snap2.instructions_executed);
    EXPECT_EQ(snap1.current_cycle, snap2.current_cycle);
    EXPECT_EQ(snap1.cpu_context.GetGpr(cpu::Reg32::EAX), snap2.cpu_context.GetGpr(cpu::Reg32::EAX));
    EXPECT_EQ(snap1.cpu_context.GetGpr(cpu::Reg32::EBX), snap2.cpu_context.GetGpr(cpu::Reg32::EBX));
    EXPECT_EQ(snap1.cpu_context.eip, snap2.cpu_context.eip);
    EXPECT_EQ(snap1.last_stop_reason.code, snap2.last_stop_reason.code);
}

int main() {
    return xblob::testing::RunAllTests();
}
