#include "tests/test_framework.hpp"
#include "xblob/common/error.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"

using namespace xblob;
using namespace xblob::cpu;
using namespace xblob::memory;

// Task 4.1: Estado arquitetural IA-32, reset, invariante do bit 1 em EFLAGS
TEST_CASE(TestCpuContextAndReset) {
    Cpu cpu;
    EXPECT_EQ(cpu.lifecycle(), CpuLifecycle::Running);
    EXPECT_EQ(cpu.context().eip, 0U);
    EXPECT_EQ(cpu.context().eflags, kFlagReserved1);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagReserved1));

    // Forçar escrita com bit 1 limpo: invariante deve reativá-lo
    cpu.context().SetEflags(0x00000000);
    EXPECT_EQ(cpu.context().eflags, kFlagReserved1);

    cpu.context().SetFlag(kFlagReserved1, false);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagReserved1));

    // Modificar registradores e resetar
    cpu.context().SetGpr(Reg32::EAX, 0x12345678);
    cpu.context().SetGpr(Reg32::ESP, 0x00100000);
    cpu.context().SetSegment(SegmentReg::CS, 0x0010);
    cpu.context().eip = 0x8000;
    cpu.Reset();

    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESP), 0U);
    EXPECT_EQ(cpu.context().GetSegment(SegmentReg::CS), 0U);
    EXPECT_EQ(cpu.context().eip, 0U);
    EXPECT_EQ(cpu.context().eflags, kFlagReserved1);
    EXPECT_EQ(cpu.lifecycle(), CpuLifecycle::Running);
}

// Task 4.2: Fetch transacional de NOP, HLT e MOV r32, imm32 com avanço atômico e falhas de
// limite/permissão
TEST_CASE(TestCpuNopHltAndMov) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::ReadExecute).has_value());

    // Código:
    // 0x1000: NOP (0x90)
    // 0x1001: MOV EBX, 0xDEADBEEF (0xBB, 0xEF, 0xBE, 0xAD, 0xDE)
    // 0x1006: HLT (0xF4)
    u8 code[] = {
        0x90,                         // NOP
        0xBB, 0xEF, 0xBE, 0xAD, 0xDE, // MOV EBX, 0xDEADBEEF
        0xF4                          // HLT
    };
    EXPECT_TRUE(ram.WriteBytes(0, ByteSpan{code, sizeof(code)}).has_value());

    Cpu cpu;
    cpu.context().eip = 0x1000;

    // Step 1: NOP
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(s1.cycles_consumed, 1ULL);
    EXPECT_EQ(cpu.context().eip, 0x1001U);

    // Step 2: MOV EBX, imm32
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_EQ(s2.cycles_consumed, 1ULL);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EBX), 0xDEADBEEFU);
    EXPECT_EQ(cpu.context().eip, 0x1006U);

    // Step 3: HLT
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Halted);
    EXPECT_EQ(s3.cycles_consumed, 1ULL);
    EXPECT_EQ(cpu.lifecycle(), CpuLifecycle::Halted);
    EXPECT_EQ(cpu.context().eip, 0x1007U);

    // Imediato truncado no fim da região executável: sem mutação parcial
    // Mapeia região executável de 3 bytes em 0x3000
    EXPECT_TRUE(mem.MapRam(0x3000, 3, ram, 0x500, MemoryPermission::ReadExecute).has_value());
    // Coloca MOV EAX (0xB8) que requer 4 bytes de imediato, mas a região só tem 3 bytes total
    EXPECT_TRUE(ram.Write8(0x500, 0xB8).has_value());

    Cpu cpu2;
    cpu2.context().eip = 0x3000;
    cpu2.context().SetGpr(Reg32::EAX, 0x11223344);

    auto s_trunc = cpu2.Step(mem);
    EXPECT_EQ(s_trunc.result, StepResult::Faulted);
    EXPECT_EQ(cpu2.lifecycle(), CpuLifecycle::Faulted);
    // EIP e registradores intactos (sem mutação parcial)
    EXPECT_EQ(cpu2.context().eip, 0x3000U);
    EXPECT_EQ(cpu2.context().GetGpr(Reg32::EAX), 0x11223344U);

    // Fetch em página não executável
    EXPECT_TRUE(mem.MapRam(0x4000, 0x100, ram, 0x600, MemoryPermission::Read).has_value());
    Cpu cpu3;
    cpu3.context().eip = 0x4000;
    auto s_no_exec = cpu3.Step(mem);
    EXPECT_EQ(s_no_exec.result, StepResult::Faulted);
    EXPECT_EQ(cpu3.lifecycle(), CpuLifecycle::Faulted);
    EXPECT_EQ(cpu3.last_fault()->code, ErrorCode::AccessViolation);
    EXPECT_EQ(cpu3.context().eip, 0x4000U);
}

// Task 4.3: ADD EAX, imm32 e SUB EAX, imm32 com cálculo de CF, PF, AF, ZF, SF e OF
TEST_CASE(TestCpuAddAndSubFlags) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::ReadExecute).has_value());

    // Cenário 1: 0x7FFFFFFF + 1 -> 0x80000000, OF=1, SF=1, CF=0
    u8 code_add[] = {
        0x05, 0x01, 0x00, 0x00, 0x00 // ADD EAX, 1
    };
    EXPECT_TRUE(ram.WriteBytes(0, ByteSpan{code_add, sizeof(code_add)}).has_value());

    Cpu cpu;
    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::EAX, 0x7FFFFFFF);

    auto step_add = cpu.Step(mem);
    EXPECT_EQ(step_add.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x80000000U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagOF));
    EXPECT_TRUE(cpu.context().GetFlag(kFlagSF));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagCF));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagZF));
    EXPECT_TRUE(cpu.context().GetFlag(kFlagReserved1));

    // Cenário 2: Subtração com borrow: 0 - 1 -> 0xFFFFFFFF, CF=1, SF=1, ZF=0
    u8 code_sub[] = {
        0x2D, 0x01, 0x00, 0x00, 0x00 // SUB EAX, 1
    };
    EXPECT_TRUE(ram.WriteBytes(0x10, ByteSpan{code_sub, sizeof(code_sub)}).has_value());

    Cpu cpu_sub;
    cpu_sub.context().eip = 0x1010;
    cpu_sub.context().SetGpr(Reg32::EAX, 0);

    auto step_sub = cpu_sub.Step(mem);
    EXPECT_EQ(step_sub.result, StepResult::Ok);
    EXPECT_EQ(cpu_sub.context().GetGpr(Reg32::EAX), 0xFFFFFFFFU);
    EXPECT_TRUE(cpu_sub.context().GetFlag(kFlagCF));
    EXPECT_TRUE(cpu_sub.context().GetFlag(kFlagSF));
    EXPECT_FALSE(cpu_sub.context().GetFlag(kFlagZF));
    EXPECT_FALSE(cpu_sub.context().GetFlag(kFlagOF));
    EXPECT_TRUE(cpu_sub.context().GetFlag(kFlagReserved1));

    // Cenário 3: 5 - 5 -> 0, ZF=1, CF=0, SF=0, OF=0, PF=1 (0 tem 0 bits 1, número par -> PF=1)
    Cpu cpu_sub_zero;
    cpu_sub_zero.context().eip = 0x1010;
    cpu_sub_zero.context().SetGpr(Reg32::EAX, 1);
    auto step_sub_zero = cpu_sub_zero.Step(mem);
    EXPECT_EQ(step_sub_zero.result, StepResult::Ok);
    EXPECT_EQ(cpu_sub_zero.context().GetGpr(Reg32::EAX), 0U);
    EXPECT_TRUE(cpu_sub_zero.context().GetFlag(kFlagZF));
    EXPECT_FALSE(cpu_sub_zero.context().GetFlag(kFlagCF));
    EXPECT_TRUE(cpu_sub_zero.context().GetFlag(kFlagPF));
}

// Task 4.4: JMP rel8 e JMP rel32 com deslocamento com sinal e wrap modular de 32 bits
TEST_CASE(TestCpuJmpRel8AndRel32) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::ReadExecute).has_value());

    // Salto rel8 positivo: em 0x1000: JMP +4 (0xEB, 0x04)
    // Destino: 0x1000 + 2 + 4 = 0x1006
    u8 jmp_forward[] = {0xEB, 0x04};
    EXPECT_TRUE(ram.WriteBytes(0, ByteSpan{jmp_forward, sizeof(jmp_forward)}).has_value());

    Cpu cpu;
    cpu.context().eip = 0x1000;
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x1006U);

    // Salto rel8 negativo: em 0x1006: JMP -8 (0xEB, 0xF8)
    // Destino: 0x1006 + 2 + (-8) = 0x1000
    u8 jmp_backward[] = {0xEB, static_cast<u8>(-8)};
    EXPECT_TRUE(ram.WriteBytes(6, ByteSpan{jmp_backward, sizeof(jmp_backward)}).has_value());

    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x1000U);

    // Salto rel32: em 0x1020: JMP +0x00000100 (0xE9, 0x00, 0x01, 0x00, 0x00)
    // Destino: 0x1020 + 5 + 0x100 = 0x1125
    u8 jmp32[] = {0xE9, 0x00, 0x01, 0x00, 0x00};
    EXPECT_TRUE(ram.WriteBytes(0x20, ByteSpan{jmp32, sizeof(jmp32)}).has_value());

    cpu.context().eip = 0x1020;
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().eip, 0x1125U);

    // Salto rel32 com wrap modular de 32 bits: destino cruza 0xFFFFFFFF
    // eip = 0xFFFFFFF0, JMP rel32 (+0x100) -> (0xFFFFFFF0 + 5 + 0x100) = 0x000000F5
    EXPECT_TRUE(
        mem.MapRam(0xFFFFFF00, 0x100, ram, 0x800, MemoryPermission::ReadExecute).has_value());
    EXPECT_TRUE(ram.WriteBytes(0x8F0, ByteSpan{jmp32, sizeof(jmp32)}).has_value());
    Cpu cpu_wrap;
    cpu_wrap.context().eip = 0xFFFFFFF0;
    auto s_wrap = cpu_wrap.Step(mem);
    EXPECT_EQ(s_wrap.result, StepResult::Ok);
    EXPECT_EQ(cpu_wrap.context().eip, 0xFFFFFFF0U + 5U + 0x100U); // Wrap de uint32_t
}

// Task 4.5: Opcode inválido com preservação de EIP e distinção de falha de fetch
TEST_CASE(TestCpuInvalidOpcodeAndDiagnostics) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x1000, 0x100, ram, 0, MemoryPermission::ReadExecute).has_value());

    // 0xCC (INT3 ou opcode não suportado no subconjunto)
    EXPECT_TRUE(ram.Write8(0, 0xCC).has_value());

    Cpu cpu;
    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::EAX, 0xCAFE);

    auto res = cpu.Step(mem);
    EXPECT_EQ(res.result, StepResult::Faulted);
    EXPECT_EQ(cpu.lifecycle(), CpuLifecycle::Faulted);
    EXPECT_TRUE(cpu.last_fault().has_value());
    EXPECT_EQ(cpu.last_fault()->code, ErrorCode::InvalidOpcode);

    // EIP preservado no endereço da instrução que falhou para diagnóstico
    EXPECT_EQ(cpu.context().eip, 0x1000U);
    // Registradores intactos
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0xCAFEU);
}

// Task 4.6: Tabela determinística de ciclos e runner com orçamentos de instrução e ciclo
TEST_CASE(TestCpuRunnerAndBudgets) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x1000, 0x100, ram, 0, MemoryPermission::ReadExecute).has_value());

    // Programa: 3 NOPs seguidos de HLT
    u8 prog[] = {0x90, 0x90, 0x90, 0xF4};
    EXPECT_TRUE(ram.WriteBytes(0, ByteSpan{prog, sizeof(prog)}).has_value());

    // Execução completa até HLT
    Cpu cpu1;
    cpu1.context().eip = 0x1000;
    auto outcome1 = cpu1.RunWithBudget(mem, 100, 100);
    EXPECT_EQ(outcome1.status, RunStatus::Halted);
    EXPECT_EQ(outcome1.instructions_executed, 4ULL);
    EXPECT_EQ(outcome1.cycles_consumed, 4ULL);
    EXPECT_EQ(cpu1.context().eip, 0x1004U);

    // Esgotamento por orçamento de instruções (limite = 2 instruções)
    Cpu cpu2;
    cpu2.context().eip = 0x1000;
    auto outcome2 = cpu2.RunWithBudget(mem, 2, 100);
    EXPECT_EQ(outcome2.status, RunStatus::BudgetExhausted);
    EXPECT_EQ(outcome2.instructions_executed, 2ULL);
    EXPECT_EQ(outcome2.cycles_consumed, 2ULL);
    EXPECT_EQ(cpu2.lifecycle(), CpuLifecycle::Running);
    EXPECT_EQ(cpu2.context().eip, 0x1002U);

    // Retomada a partir do estado anterior
    auto resume = cpu2.RunWithBudget(mem, 10, 10);
    EXPECT_EQ(resume.status, RunStatus::Halted);
    EXPECT_EQ(resume.instructions_executed, 2ULL); // 1 NOP restante + 1 HLT
    EXPECT_EQ(resume.cycles_consumed, 2ULL);
    EXPECT_EQ(cpu2.context().eip, 0x1004U);
}

int main() {
    return xblob::testing::RunAllTests();
}
