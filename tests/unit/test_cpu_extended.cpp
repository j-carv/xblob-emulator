#include "tests/test_framework.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/cpu/decoder.hpp"
#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/memory/virtual_memory.hpp"

using namespace xblob;
using namespace xblob::cpu;
using namespace xblob::memory;

TEST_CASE(TestModRmAndSib) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    cpu.context().SetGpr(Reg32::EAX, 0x1000);
    cpu.context().SetGpr(Reg32::ECX, 0x20);
    cpu.context().SetGpr(Reg32::EDX, 0x4);
    cpu.context().eip = 0x100;

    // 1. SIB with base=EAX, index=ECX, scale=2 (*4), disp8=0x10
    // MOV EBX, [EAX + ECX*4 + 0x10]
    // opcode: 8B /r -> ModR/M: mod=01 (disp8), reg=011 (EBX), rm=100 (SIB) -> 0x5C
    // SIB: scale=10 (x4), index=001 (ECX), base=000 (EAX) -> 0x88
    // disp8: 0x10
    // target EA = 0x1000 + 0x20*4 + 0x10 = 0x1000 + 0x80 + 0x10 = 0x1090
    EXPECT_TRUE(mem.Write8(0x100, 0x8B).has_value());
    EXPECT_TRUE(mem.Write8(0x101, 0x5C).has_value());
    EXPECT_TRUE(mem.Write8(0x102, 0x88).has_value());
    EXPECT_TRUE(mem.Write8(0x103, 0x10).has_value());
    EXPECT_TRUE(mem.Write32(0x1090, 0x12345678).has_value());

    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EBX), 0x12345678U);
    EXPECT_EQ(cpu.context().eip, 0x104U);

    // 2. SIB with index=none (index=4, ESP), base=EDX, disp32
    // MOV ESI, [EDX + 0x200]
    // opcode 8B /r -> ModR/M: mod=10 (disp32), reg=110 (ESI), rm=100 (SIB) -> 0xB4
    // SIB: scale=00, index=100 (none), base=010 (EDX) -> 0x22
    // disp32 = 0x00000200
    // target EA = 0x4 + 0x200 = 0x204
    EXPECT_TRUE(mem.Write8(0x104, 0x8B).has_value());
    EXPECT_TRUE(mem.Write8(0x105, 0xB4).has_value());
    EXPECT_TRUE(mem.Write8(0x106, 0x22).has_value());
    EXPECT_TRUE(mem.Write32(0x107, 0x200).has_value());
    EXPECT_TRUE(mem.Write32(0x204, 0xCAFEBABE).has_value());

    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0xCAFEBABEU);
    EXPECT_EQ(cpu.context().eip, 0x10BU);
}

TEST_CASE(TestAluAndFlags) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    cpu.context().eip = 0x200;

    // ADD EAX, 5 (0x05 05 00 00 00)
    cpu.context().SetGpr(Reg32::EAX, 10);
    EXPECT_TRUE(mem.Write8(0x200, 0x05).has_value());
    EXPECT_TRUE(mem.Write32(0x201, 5).has_value());

    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 15U);
    EXPECT_TRUE(!cpu.context().GetFlag(kFlagZF));
    EXPECT_TRUE(!cpu.context().GetFlag(kFlagCF));

    // SUB EAX, 15 (0x2D 0F 00 00 00) -> result 0, ZF=1
    EXPECT_TRUE(mem.Write8(0x205, 0x2D).has_value());
    EXPECT_TRUE(mem.Write32(0x206, 15).has_value());

    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagZF));
    EXPECT_TRUE(!cpu.context().GetFlag(kFlagCF));

    // DEC EAX (0x48) -> 0 - 1 = 0xFFFFFFFF, SF=1, CF preserved (0)
    EXPECT_TRUE(mem.Write8(0x20A, 0x48).has_value());
    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0xFFFFFFFFU);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagSF));
    EXPECT_TRUE(!cpu.context().GetFlag(kFlagCF));
}

TEST_CASE(TestStackAndBranches) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    cpu.context().SetGpr(Reg32::ESP, 0x8000);
    cpu.context().eip = 0x300;

    // CALL rel32 to 0x320
    // Opcode: E8 disp32 -> target = (0x300 + 5) + disp32 = 0x320 => disp32 = 0x1B
    EXPECT_TRUE(mem.Write8(0x300, 0xE8).has_value());
    EXPECT_TRUE(mem.Write32(0x301, 0x1B).has_value());

    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x320U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x7FFCU);

    // At 0x320: PUSH 0x42 (0x6A 0x42)
    EXPECT_TRUE(mem.Write8(0x320, 0x6A).has_value());
    EXPECT_TRUE(mem.Write8(0x321, 0x42).has_value());
    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x7FF8U);

    // POP EBX (0x5B)
    EXPECT_TRUE(mem.Write8(0x322, 0x5B).has_value());
    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EBX), 0x42U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x7FFCU);

    // RET (0xC3)
    EXPECT_TRUE(mem.Write8(0x323, 0xC3).has_value());
    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x305U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x8000U);
}

TEST_CASE(TestIdtInterruptAndIret) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    cpu.context().SetGpr(Reg32::ESP, 0x9000);
    cpu.context().SetSegment(SegmentReg::CS, 0x08);
    cpu.context().eip = 0x400;

    // Setup IDT at 0x1000
    cpu.context().idtr.base = 0x1000;
    cpu.context().idtr.limit = 0x7FF; // 256 gates

    // Setup Interrupt Gate for vector 0x20 at 0x1000 + 0x20*8 = 0x1100
    // Handler at 0x500, Selector 0x08, Type 0x8E (P=1, DPL=0, S=0, Type=0xE)
    const GuestAddr gate_addr = 0x1100;
    EXPECT_TRUE(mem.Write16(gate_addr + 0, 0x0500).has_value()); // offset_low
    EXPECT_TRUE(mem.Write16(gate_addr + 2, 0x0008).has_value()); // selector
    EXPECT_TRUE(mem.Write16(gate_addr + 4, 0x8E00).has_value()); // type_attr (P=1, Type=0xE)
    EXPECT_TRUE(mem.Write16(gate_addr + 6, 0x0000).has_value()); // offset_high

    // Put handler at 0x500: IRET (0xCF)
    EXPECT_TRUE(mem.Write8(0x500, 0xCF).has_value());

    // Deliver interrupt 0x20
    cpu.context().SetFlag(kFlagIF, true);
    auto del_res = Executor::DeliverGate(0x20, cpu.context(), mem, std::nullopt, false, 0x400);
    EXPECT_TRUE(del_res.has_value());
    EXPECT_EQ(cpu.context().eip, 0x500U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x9000U - 12U);
    EXPECT_TRUE(!cpu.context().GetFlag(kFlagIF)); // Interrupt gate clears IF

    // Execute IRET at 0x500
    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x400U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0x9000U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagIF)); // Restored from pushed EFLAGS
}

TEST_CASE(TestInstructionLimit) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    cpu.context().eip = 0x600;

    // Fill with opcode stream exceeding 15 bytes
    for (u32 i = 0; i < 20; ++i) {
        EXPECT_TRUE(mem.Write8(0x600 + i, 0x0F).has_value());
    }

    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Faulted);
    EXPECT_EQ(cpu.lifecycle(), CpuLifecycle::Faulted);
    EXPECT_TRUE(cpu.last_exception().has_value());
    EXPECT_EQ(cpu.last_exception()->vector, ExceptionVector::InvalidOpcode);
}

class MockIrqSource : public IInterruptSource {
public:
    void SetPending(u8 vector) {
        has_pending_ = true;
        vector_ = vector;
    }

    [[nodiscard]] bool HasPendingInterrupt() const noexcept override { return has_pending_; }

    [[nodiscard]] std::optional<u8> AcknowledgeInterrupt() noexcept override {
        if (!has_pending_)
            return std::nullopt;
        has_pending_ = false;
        return vector_;
    }

private:
    bool has_pending_{false};
    u8 vector_{0};
};

TEST_CASE(TestInterruptBoundaryAndIf) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;
    MockIrqSource irq;
    cpu.SetInterruptSource(&irq);
    cpu.context().eip = 0x700;
    cpu.context().SetGpr(Reg32::ESP, 0x8000);
    cpu.context().idtr.base = 0x2000;
    cpu.context().idtr.limit = 0x7FF;

    // Gate for vector 0x30 at 0x2000 + 0x30*8 = 0x2180 -> handler at 0x800
    const GuestAddr gate_addr = 0x2180;
    EXPECT_TRUE(mem.Write16(gate_addr + 0, 0x0800).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 2, 0x0008).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 4, 0x8E00).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 6, 0x0000).has_value());

    // NOP at 0x700
    EXPECT_TRUE(mem.Write8(0x700, 0x90).has_value());

    // 1. IF=0: IRQ pending should NOT be delivered
    cpu.context().SetFlag(kFlagIF, false);
    irq.SetPending(0x30);

    auto step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x701U);   // NOP executed, no IRQ taken
    EXPECT_TRUE(irq.HasPendingInterrupt()); // Still pending

    // 2. IF=1: IRQ pending MUST be delivered at instruction boundary
    cpu.context().SetFlag(kFlagIF, true);
    step = cpu.Step(mem);
    EXPECT_EQ(step.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x800U); // Delivered to handler
    EXPECT_TRUE(!irq.HasPendingInterrupt());
}

TEST_CASE(TestIdtValidationFailures) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    CpuContext ctx;
    ctx.idtr.base = 0x1000;
    ctx.idtr.limit = 0x7F; // only 16 gates (0..15)

    // Vector 16 exceeds limit -> #GP
    auto r1 = Executor::DeliverGate(16, ctx, mem, std::nullopt, false, 0x100);
    EXPECT_TRUE(!r1.has_value());
    EXPECT_EQ(r1.error().code, ErrorCode::GeneralProtection);

    // Vector 5 within limit, but gate is zeroed (Present=0) -> #NP
    auto r2 = Executor::DeliverGate(5, ctx, mem, std::nullopt, false, 0x100);
    EXPECT_TRUE(!r2.has_value());
    EXPECT_EQ(r2.error().code, ErrorCode::SegmentNotPresent);

    // Present=1, but Type=0x0 (invalid) -> #GP
    const GuestAddr g5 = 0x1000 + 5 * 8;
    EXPECT_TRUE(mem.Write16(g5 + 0, 0x0500).has_value());
    EXPECT_TRUE(mem.Write16(g5 + 2, 0x0008).has_value());
    EXPECT_TRUE(mem.Write16(g5 + 4, 0x8000).has_value()); // P=1, S=0, Type=0
    EXPECT_TRUE(mem.Write16(g5 + 6, 0x0000).has_value());

    auto r3 = Executor::DeliverGate(5, ctx, mem, std::nullopt, false, 0x100);
    EXPECT_TRUE(!r3.has_value());
    EXPECT_EQ(r3.error().code, ErrorCode::GeneralProtection);
}

TEST_CASE(TestTransactionalFrameRollbackOnStackFault) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    // Map only 0x1000..0x2000; 0x0000..0x0FFF is UNMAPPED
    EXPECT_TRUE(mem.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::All).has_value());

    CpuContext ctx;
    ctx.eip = 0x1200;
    ctx.SetGpr(Reg32::ESP, 0x1004); // ESP - 12 = 0x0FF8 (UNMAPPED!)
    ctx.SetSegment(SegmentReg::CS, 0x0008);
    ctx.SetFlag(kFlagIF, true);

    ctx.idtr.base = 0x1400;
    ctx.idtr.limit = 0x7FF;

    // Gate for vector 0x20
    const GuestAddr gate_addr = 0x1400 + 0x20 * 8;
    EXPECT_TRUE(mem.Write16(gate_addr + 0, 0x1800).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 2, 0x0008).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 4, 0x8E00).has_value());
    EXPECT_TRUE(mem.Write16(gate_addr + 6, 0x0000).has_value());

    // Delivery must fail due to unmapped stack
    auto del_res = Executor::DeliverGate(0x20, ctx, mem, std::nullopt, false, 0x1200);
    EXPECT_TRUE(!del_res.has_value());

    // ZERO PARTIAL MUTATION:
    EXPECT_EQ(ctx.eip, 0x1200U);
    EXPECT_EQ(ctx.GetGpr(Reg32::ESP), 0x1004U);
    EXPECT_EQ(ctx.GetSegment(SegmentReg::CS), 0x0008U);
    EXPECT_TRUE(ctx.GetFlag(kFlagIF));
}

TEST_CASE(TestIretPrivilegeRejection) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    CpuContext ctx;
    ctx.SetSegment(SegmentReg::CS, 0x0008); // CPL = 0
    ctx.SetGpr(Reg32::ESP, 0x5000);

    // Frame on stack:
    // [ESP+0] EIP = 0x1000
    // [ESP+4] CS = 0x001B (RPL = 3 -> privilege transition!)
    // [ESP+8] EFLAGS = 0x00000002
    EXPECT_TRUE(mem.Write32(0x5000, 0x1000).has_value());
    EXPECT_TRUE(mem.Write32(0x5004, 0x001B).has_value());
    EXPECT_TRUE(mem.Write32(0x5008, 0x0002).has_value());

    auto res = Executor::ExecuteIret(ctx, mem);
    EXPECT_TRUE(!res.has_value());
    EXPECT_EQ(res.error().code, ErrorCode::GeneralProtection);

    // ESP must remain unchanged on rejected transition
    EXPECT_EQ(ctx.GetGpr(Reg32::ESP), 0x5000U);
}

#include "test_cpu_expansion_part1.inl"
#include "test_cpu_expansion_part2.inl"

int main() {
    return xblob::testing::RunAllTests();
}
