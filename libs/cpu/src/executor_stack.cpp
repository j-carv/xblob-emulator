#include "xblob/cpu/executor.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteStack(const DecodedInstruction& inst, CpuContext& ctx,
                                     MemoryType& mem, u32 current_eip) {
    ExecutionResult res;
    const u32 esp = ctx.GetGpr(Reg32::ESP);

    if (inst.id == InstructionId::Push) {
        auto val_res = inst.op1.Read(ctx, mem);
        if (!val_res)
            return val_res.error();
        const u32 val = *val_res;

        const u32 new_esp = esp - 4U;
        auto w_res = mem.Write32(new_esp, val);
        if (!w_res)
            return w_res.error();

        ctx.SetGpr(Reg32::ESP, new_esp);
        return res;
    }

    if (inst.id == InstructionId::Pop) {
        auto val_res = mem.Read32(esp);
        if (!val_res)
            return val_res.error();
        const u32 val = *val_res;

        auto w_res = inst.op1.Write(ctx, mem, val);
        if (!w_res)
            return w_res.error();

        ctx.SetGpr(Reg32::ESP, esp + 4U);
        return res;
    }

    if (inst.id == InstructionId::Pushf) {
        const u32 new_esp = esp - 4U;
        auto w_res = mem.Write32(new_esp, ctx.eflags);
        if (!w_res)
            return w_res.error();

        ctx.SetGpr(Reg32::ESP, new_esp);
        return res;
    }

    if (inst.id == InstructionId::Popf) {
        auto val_res = mem.Read32(esp);
        if (!val_res)
            return val_res.error();

        ctx.SetEflags(*val_res);
        ctx.SetGpr(Reg32::ESP, esp + 4U);
        return res;
    }

    if (inst.id == InstructionId::Call) {
        u32 target = 0;
        if (inst.op1.kind == OperandKind::Immediate) {
            target = inst.branch_target;
        } else {
            auto tgt_res = inst.op1.Read(ctx, mem);
            if (!tgt_res)
                return tgt_res.error();
            target = *tgt_res;
        }

        const u32 return_eip = current_eip + inst.length;
        const u32 new_esp = esp - 4U;
        auto w_res = mem.Write32(new_esp, return_eip);
        if (!w_res)
            return w_res.error();

        ctx.SetGpr(Reg32::ESP, new_esp);
        res.branched = true;
        res.next_eip = target;
        return res;
    }

    if (inst.id == InstructionId::Ret) {
        auto ret_res = mem.Read32(esp);
        if (!ret_res)
            return ret_res.error();

        const u32 new_esp = esp + 4U + inst.ret_pop_bytes;
        ctx.SetGpr(Reg32::ESP, new_esp);
        res.branched = true;
        res.next_eip = *ret_res;
        return res;
    }

    return Error{ErrorCode::InvalidArgument, "Instrução de pilha desconhecida"};
}

} // namespace detail

// Template instantiations
template Result<ExecutionResult>
detail::ExecuteStack<memory::AddressSpace>(const DecodedInstruction& inst, CpuContext& ctx,
                                           memory::AddressSpace& mem, u32 current_eip);

template Result<ExecutionResult>
detail::ExecuteStack<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                            memory::VirtualMemory& mem, u32 current_eip);

} // namespace xblob::cpu
