#include "tests/test_framework.hpp"
#include "xblob/common/types.hpp"
#include "xblob/core/scheduler.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"

#include <vector>

using namespace xblob;
using namespace xblob::core;
using namespace xblob::cpu;
using namespace xblob::memory;

struct MachineRunResult {
    CpuContext final_context;
    Cycle final_cycle{0};
    u64 instructions_executed{0};
    std::vector<int> event_log;
    u32 memory_word{0};
};

static MachineRunResult RunSyntheticMachine() {
    auto ram_res = Ram::Create(kRamSizeRetail);
    if (!ram_res) {
        throw std::runtime_error("Falha ao alocar RAM");
    }
    Ram& ram = ram_res.value();

    AddressSpace mem;
    // Mapeamento de código: [0x1000, 0x2000)
    (void)mem.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::ReadExecute);
    // Mapeamento de dados: [0x2000, 0x3000)
    (void)mem.MapRam(0x2000, 0x1000, ram, 0x1000, MemoryPermission::ReadWrite);

    // Programa sintético em 0x1000:
    // 0x1000: MOV EAX, 0x00000064 (100)        [5 bytes]
    // 0x1005: MOV EBX, 0x00000032 (50)         [5 bytes]
    // 0x100A: ADD EAX, 0x00000019 (25)         [5 bytes]
    // 0x100F: SUB EAX, 0x0000000A (10)         [5 bytes]
    // 0x1014: NOP                               [1 byte]
    // 0x1015: HLT                               [1 byte]
    const u8 code[] = {
        0xB8, 0x64, 0x00, 0x00, 0x00, // MOV EAX, 100
        0xBB, 0x32, 0x00, 0x00, 0x00, // MOV EBX, 50
        0x05, 0x19, 0x00, 0x00, 0x00, // ADD EAX, 25
        0x2D, 0x0A, 0x00, 0x00, 0x00, // SUB EAX, 10
        0x90,                         // NOP
        0xF4                          // HLT
    };
    (void)ram.WriteBytes(0, ByteSpan{code, sizeof(code)});

    DeterministicScheduler sched(0);
    MachineRunResult result;

    // Agendar eventos no scheduler em ciclos específicos
    (void)sched.ScheduleEvent(2, [&](Cycle) {
        result.event_log.push_back(102);
        // Evento escreve na memória de dados
        (void)mem.Write32(0x2000, 0xCAFEBABE);
    });

    (void)sched.ScheduleEvent(4, [&](Cycle) { result.event_log.push_back(104); });

    (void)sched.ScheduleEvent(4, [&](Cycle) { result.event_log.push_back(105); });

    Cpu cpu;
    cpu.context().eip = 0x1000;

    while (cpu.lifecycle() == CpuLifecycle::Running) {
        StepOutcome outcome = cpu.Step(mem);
        if (outcome.result == StepResult::Faulted) {
            break;
        }

        result.instructions_executed++;

        // Avança o scheduler em sincronia com os ciclos consumidos pela CPU
        if (outcome.cycles_consumed > 0) {
            (void)sched.StepCycles(outcome.cycles_consumed);
        }

        if (outcome.result == StepResult::Halted) {
            break;
        }
    }

    result.final_context = cpu.context();
    result.final_cycle = sched.current_cycle();
    auto mem_res = mem.Read32(0x2000);
    if (mem_res) {
        result.memory_word = mem_res.value();
    }

    return result;
}

// Task 5.1: Executa a sequência duas vezes e verifica determinismo estrito
TEST_CASE(TestSyntheticMachineDeterminism) {
    MachineRunResult run1 = RunSyntheticMachine();
    MachineRunResult run2 = RunSyntheticMachine();

    // 1. Igualdade de contagem de instruções e ciclo final
    EXPECT_EQ(run1.instructions_executed, 6ULL);
    EXPECT_EQ(run1.instructions_executed, run2.instructions_executed);
    EXPECT_EQ(run1.final_cycle, run2.final_cycle);
    EXPECT_EQ(run1.final_cycle, 6ULL);

    // 2. Igualdade estrita de contexto de CPU
    EXPECT_EQ(run1.final_context.eip, 0x1016U);
    EXPECT_EQ(run1.final_context.eip, run2.final_context.eip);
    EXPECT_EQ(run1.final_context.GetGpr(Reg32::EAX), 115U); // 100 + 25 - 10 = 115
    EXPECT_EQ(run1.final_context.GetGpr(Reg32::EAX), run2.final_context.GetGpr(Reg32::EAX));
    EXPECT_EQ(run1.final_context.GetGpr(Reg32::EBX), 50U);
    EXPECT_EQ(run1.final_context.GetGpr(Reg32::EBX), run2.final_context.GetGpr(Reg32::EBX));
    EXPECT_EQ(run1.final_context.eflags, run2.final_context.eflags);

    // 3. Igualdade estrita de eventos do scheduler e ordem de despacho
    EXPECT_EQ(run1.event_log.size(), 3ULL);
    EXPECT_EQ(run1.event_log.size(), run2.event_log.size());
    for (std::size_t i = 0; i < run1.event_log.size(); ++i) {
        EXPECT_EQ(run1.event_log[i], run2.event_log[i]);
    }

    // 4. Igualdade estrita de memória modificada
    EXPECT_EQ(run1.memory_word, 0xCAFEBABEU);
    EXPECT_EQ(run1.memory_word, run2.memory_word);
}

int main() {
    return xblob::testing::RunAllTests();
}
