#include "xblob/cpu/executor.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteBranch(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip) {
    (void)current_eip;
    ExecutionResult res;

    if (inst.id == InstructionId::Jmp) {
        u32 target = 0;
        if (inst.op1.kind == OperandKind::Immediate) {
            target = inst.branch_target;
        } else {
            auto tgt_res = inst.op1.Read(ctx, mem);
            if (!tgt_res)
                return tgt_res.error();
            target = *tgt_res;
        }
        res.branched = true;
        res.next_eip = target;
        return res;
    }

    if (inst.id == InstructionId::Jcc) {
        if (EvaluateCondition(inst.condition, ctx.eflags)) {
            res.branched = true;
            res.next_eip = inst.branch_target;
        }
        return res;
    }

    return Error{ErrorCode::InvalidArgument, "Instrução de salto desconhecida"};
}

} // namespace detail

// Template instantiations
template Result<ExecutionResult>
detail::ExecuteBranch<memory::AddressSpace>(const DecodedInstruction& inst, CpuContext& ctx,
                                            memory::AddressSpace& mem, u32 current_eip);

template Result<ExecutionResult>
detail::ExecuteBranch<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                             memory::VirtualMemory& mem, u32 current_eip);

} // namespace xblob::cpu
