#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteAtomic(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem) {
    if (inst.id == InstructionId::Xchg) {
        auto v1_res = inst.op1.Read(ctx, mem);
        if (!v1_res) {
            return v1_res.error();
        }
        auto v2_res = inst.op2.Read(ctx, mem);
        if (!v2_res) {
            return v2_res.error();
        }

        auto w1_res = inst.op1.Write(ctx, mem, *v2_res);
        if (!w1_res) {
            return w1_res.error();
        }
        auto w2_res = inst.op2.Write(ctx, mem, *v1_res);
        if (!w2_res) {
            return w2_res.error();
        }

        return ExecutionResult{};
    }

    if (inst.id == InstructionId::Cmpxchg) {
        auto dest_res = inst.op1.Read(ctx, mem);
        if (!dest_res) {
            return dest_res.error();
        }
        auto src_res = inst.op2.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }

        if (inst.data_size == 1) {
            const u8 accum = ctx.GetGpr8(0); // AL
            const u8 dest_val = static_cast<u8>(*dest_res & 0xFFU);
            ctx.eflags = CalculateSubFlags8(accum, dest_val, ctx.eflags);

            if (accum == dest_val) {
                auto w_res = inst.op1.Write(ctx, mem, *src_res & 0xFFU);
                if (!w_res) {
                    return w_res.error();
                }
            } else {
                ctx.SetGpr8(0, dest_val);
            }
        } else {
            const u32 accum = ctx.GetGpr(Reg32::EAX);
            const u32 dest_val = *dest_res;
            ctx.eflags = CalculateSubFlags(accum, dest_val, ctx.eflags);

            if (accum == dest_val) {
                auto w_res = inst.op1.Write(ctx, mem, *src_res);
                if (!w_res) {
                    return w_res.error();
                }
            } else {
                ctx.SetGpr(Reg32::EAX, dest_val);
            }
        }

        return ExecutionResult{};
    }

    return Error{ErrorCode::InvalidOpcode, "Operação atômica desconhecida"};
}

template Result<ExecutionResult> ExecuteAtomic<memory::AddressSpace>(const DecodedInstruction& inst,
                                                                     CpuContext& ctx,
                                                                     memory::AddressSpace& mem);
template Result<ExecutionResult>
ExecuteAtomic<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                     memory::VirtualMemory& mem);

} // namespace xblob::cpu::detail
