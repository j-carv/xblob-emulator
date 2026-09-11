#include "xblob/cpu/executor.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteInterruptInstruction(const DecodedInstruction& inst, CpuContext& ctx,
                                                    MemoryType& mem, u32 current_eip,
                                                    const TrapHandler& trap) {
    ExecutionResult res;

    if (inst.id == InstructionId::Cli) {
        ctx.SetFlag(kFlagIF, false);
        return res;
    }

    if (inst.id == InstructionId::Sti) {
        ctx.SetFlag(kFlagIF, true);
        return res;
    }

    if (inst.id == InstructionId::Iret) {
        auto iret_res = Executor::ExecuteIret(ctx, mem);
        if (!iret_res) {
            return iret_res.error();
        }
        res.branched = true;
        res.next_eip = ctx.eip;
        return res;
    }

    if (inst.id == InstructionId::Int) {
        if (trap) {
            auto trap_res = trap(inst.int_vector, ctx);
            if (!trap_res) {
                return trap_res.error();
            }
            if (*trap_res) {
                res.branched = true;
                res.next_eip = ctx.eip;
                return res;
            }
        }

        const u32 return_eip = current_eip + inst.length;
        auto gate_res =
            Executor::DeliverGate(inst.int_vector, ctx, mem, std::nullopt, true, return_eip);
        if (!gate_res) {
            return gate_res.error();
        }
        res.branched = true;
        res.next_eip = ctx.eip;
        return res;
    }

    return Error{ErrorCode::InvalidArgument, "Instrução de controle/interrupção desconhecida"};
}

} // namespace detail

template <typename MemoryType>
Result<void> Executor::DeliverGate(u8 vector, CpuContext& ctx, MemoryType& mem,
                                   std::optional<u32> error_code, bool is_software_int,
                                   u32 return_eip) {
    const u32 gate_offset = static_cast<u32>(vector) * 8U;
    if (gate_offset + 7U > ctx.idtr.limit) {
        return Error{ErrorCode::GeneralProtection, "Vetor excede limite da IDTR",
                     static_cast<u32>(vector * 8U + 2U)};
    }

    const GuestAddr gate_addr = ctx.idtr.base + gate_offset;
    auto offset_low_res = mem.Read16(gate_addr);
    if (!offset_low_res)
        return offset_low_res.error();

    auto sel_res = mem.Read16(gate_addr + 2U);
    if (!sel_res)
        return sel_res.error();

    auto attr_res = mem.Read16(gate_addr + 4U);
    if (!attr_res)
        return attr_res.error();

    auto offset_high_res = mem.Read16(gate_addr + 6U);
    if (!offset_high_res)
        return offset_high_res.error();

    const u16 attr = *attr_res;
    const bool present = ((attr >> 15) & 1) != 0;
    const u8 dpl = static_cast<u8>((attr >> 13) & 3);
    const bool system_desc = ((attr >> 12) & 1) == 0;
    const u8 gate_type = static_cast<u8>((attr >> 8) & 0x0F);

    if (!present) {
        return Error{ErrorCode::SegmentNotPresent, "Gate de interrupção não presente",
                     static_cast<u32>(vector * 8U + 2U)};
    }

    if (!system_desc) {
        return Error{ErrorCode::GeneralProtection, "Gate de interrupção com bit S != 0",
                     static_cast<u32>(vector * 8U + 2U)};
    }

    // IA-32: 0xE = 32-bit Interrupt Gate, 0xF = 32-bit Trap Gate
    if (gate_type != 0x0E && gate_type != 0x0F) {
        return Error{ErrorCode::GeneralProtection, "Tipo de gate inválido",
                     static_cast<u32>(vector * 8U + 2U)};
    }

    if (is_software_int) {
        const u8 cpl = static_cast<u8>(ctx.GetSegment(SegmentReg::CS) & 3);
        if (cpl > dpl) {
            return Error{ErrorCode::GeneralProtection, "Privilégio insuficiente para INT n",
                         static_cast<u32>(vector * 8U + 2U)};
        }
    }

    const u32 target_handler =
        (static_cast<u32>(*offset_high_res) << 16) | static_cast<u32>(*offset_low_res);
    const u32 esp = ctx.GetGpr(Reg32::ESP);

    // Transactional frame creation
    if (error_code.has_value()) {
        const u32 new_esp = esp - 16U;
        auto w1 = mem.Write32(esp - 4U, ctx.eflags);
        if (!w1)
            return w1.error();
        auto w2 = mem.Write32(esp - 8U, static_cast<u32>(ctx.GetSegment(SegmentReg::CS)));
        if (!w2)
            return w2.error();
        auto w3 = mem.Write32(esp - 12U, return_eip);
        if (!w3)
            return w3.error();
        auto w4 = mem.Write32(esp - 16U, *error_code);
        if (!w4)
            return w4.error();

        ctx.SetGpr(Reg32::ESP, new_esp);
    } else {
        const u32 new_esp = esp - 12U;
        auto w1 = mem.Write32(esp - 4U, ctx.eflags);
        if (!w1)
            return w1.error();
        auto w2 = mem.Write32(esp - 8U, static_cast<u32>(ctx.GetSegment(SegmentReg::CS)));
        if (!w2)
            return w2.error();
        auto w3 = mem.Write32(esp - 12U, return_eip);
        if (!w3)
            return w3.error();

        ctx.SetGpr(Reg32::ESP, new_esp);
    }

    ctx.SetSegment(SegmentReg::CS, *sel_res);
    ctx.eip = target_handler;

    // Interrupt gate clears IF and TF; Trap gate preserves IF
    if (gate_type == 0x0E) {
        ctx.SetFlag(kFlagIF, false);
        ctx.SetFlag(kFlagTF, false);
    }

    return {};
}

template <typename MemoryType>
Result<void> Executor::ExecuteIret(CpuContext& ctx, MemoryType& mem) {
    const u32 esp = ctx.GetGpr(Reg32::ESP);

    auto eip_res = mem.Read32(esp);
    if (!eip_res)
        return eip_res.error();

    auto cs_res = mem.Read32(esp + 4U);
    if (!cs_res)
        return cs_res.error();

    auto eflags_res = mem.Read32(esp + 8U);
    if (!eflags_res)
        return eflags_res.error();

    const u8 current_cpl = static_cast<u8>(ctx.GetSegment(SegmentReg::CS) & 3);
    const u8 return_rpl = static_cast<u8>(*cs_res & 3);

    // Reject privilege transitions in initial foundation
    if (return_rpl != current_cpl) {
        return Error{ErrorCode::GeneralProtection, "Transição de privilégio em IRET não suportada",
                     *cs_res};
    }

    ctx.eip = *eip_res;
    ctx.SetSegment(SegmentReg::CS, static_cast<u16>(*cs_res & 0xFFFF));
    ctx.SetEflags(*eflags_res);
    ctx.SetGpr(Reg32::ESP, esp + 12U);

    return {};
}

// Master Dispatcher
template <typename MemoryType>
Result<ExecutionResult> Executor::Execute(const DecodedInstruction& inst, CpuContext& ctx,
                                          MemoryType& mem, u32 current_eip,
                                          const TrapHandler& trap) {
    switch (inst.id) {
    case InstructionId::Nop:
        return ExecutionResult{};
    case InstructionId::Hlt: {
        ExecutionResult r;
        r.halted = true;
        return r;
    }
    case InstructionId::Mov:
    case InstructionId::Lea:
        return detail::ExecuteData(inst, ctx, mem);
    case InstructionId::Push:
    case InstructionId::Pop:
    case InstructionId::Pushf:
    case InstructionId::Popf:
    case InstructionId::Call:
    case InstructionId::Ret:
        return detail::ExecuteStack(inst, ctx, mem, current_eip);
    case InstructionId::Add:
    case InstructionId::Adc:
    case InstructionId::Sub:
    case InstructionId::Sbb:
    case InstructionId::Cmp:
    case InstructionId::Inc:
    case InstructionId::Dec:
    case InstructionId::And:
    case InstructionId::Or:
    case InstructionId::Xor:
    case InstructionId::Test:
        return detail::ExecuteAlu(inst, ctx, mem);
    case InstructionId::Jmp:
    case InstructionId::Jcc:
        return detail::ExecuteBranch(inst, ctx, mem, current_eip);
    case InstructionId::Cli:
    case InstructionId::Sti:
    case InstructionId::Iret:
    case InstructionId::Int:
        return detail::ExecuteInterruptInstruction(inst, ctx, mem, current_eip, trap);
    default:
        return Error{ErrorCode::InvalidOpcode, "Opcode não suportado para execução"};
    }
}

// Template instantiations
template Result<void> Executor::DeliverGate<memory::AddressSpace>(u8 vector, CpuContext& ctx,
                                                                  memory::AddressSpace& mem,
                                                                  std::optional<u32> error_code,
                                                                  bool is_software_int,
                                                                  u32 return_eip);

template Result<void> Executor::DeliverGate<memory::VirtualMemory>(u8 vector, CpuContext& ctx,
                                                                   memory::VirtualMemory& mem,
                                                                   std::optional<u32> error_code,
                                                                   bool is_software_int,
                                                                   u32 return_eip);

template Result<void> Executor::ExecuteIret<memory::AddressSpace>(CpuContext& ctx,
                                                                  memory::AddressSpace& mem);

template Result<void> Executor::ExecuteIret<memory::VirtualMemory>(CpuContext& ctx,
                                                                   memory::VirtualMemory& mem);

template Result<ExecutionResult>
Executor::Execute<memory::AddressSpace>(const DecodedInstruction& inst, CpuContext& ctx,
                                        memory::AddressSpace& mem, u32 current_eip,
                                        const TrapHandler& trap);

template Result<ExecutionResult>
Executor::Execute<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                         memory::VirtualMemory& mem, u32 current_eip,
                                         const TrapHandler& trap);

} // namespace xblob::cpu
