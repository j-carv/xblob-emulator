#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteAlu(const DecodedInstruction& inst, CpuContext& ctx,
                                   MemoryType& mem) {
    ExecutionResult res;

    // Single-operand ALU: INC, DEC
    if (inst.id == InstructionId::Inc || inst.id == InstructionId::Dec) {
        auto a_res = inst.op1.Read(ctx, mem);
        if (!a_res)
            return a_res.error();
        const u32 a = *a_res;

        u32 out_val = 0;
        u32 new_flags = 0;
        if (inst.id == InstructionId::Inc) {
            out_val = a + 1;
            new_flags = CalculateIncFlags(a, ctx.eflags);
        } else {
            out_val = a - 1;
            new_flags = CalculateDecFlags(a, ctx.eflags);
        }

        auto w_res = inst.op1.Write(ctx, mem, out_val);
        if (!w_res)
            return w_res.error();

        ctx.SetEflags(new_flags);
        return res;
    }

    // Two-operand ALU / Logic
    auto a_res = inst.op1.Read(ctx, mem);
    if (!a_res)
        return a_res.error();
    auto b_res = inst.op2.Read(ctx, mem);
    if (!b_res)
        return b_res.error();

    const u32 a = *a_res;
    const u32 b = *b_res;

    u32 out_val = 0;
    u32 new_flags = 0;
    bool write_back = true;

    switch (inst.id) {
    case InstructionId::Add:
        out_val = a + b;
        new_flags = CalculateAddFlags(a, b, ctx.eflags);
        break;
    case InstructionId::Adc: {
        const bool cf = ctx.GetFlag(kFlagCF);
        out_val = a + b + (cf ? 1U : 0U);
        new_flags = CalculateAdcFlags(a, b, cf, ctx.eflags);
        break;
    }
    case InstructionId::Sub:
        out_val = a - b;
        new_flags = CalculateSubFlags(a, b, ctx.eflags);
        break;
    case InstructionId::Sbb: {
        const bool cf = ctx.GetFlag(kFlagCF);
        out_val = a - b - (cf ? 1U : 0U);
        new_flags = CalculateSbbFlags(a, b, cf, ctx.eflags);
        break;
    }
    case InstructionId::Cmp:
        out_val = a - b;
        new_flags = CalculateSubFlags(a, b, ctx.eflags);
        write_back = false;
        break;
    case InstructionId::And:
        out_val = a & b;
        new_flags = CalculateLogicFlags(out_val, ctx.eflags);
        break;
    case InstructionId::Or:
        out_val = a | b;
        new_flags = CalculateLogicFlags(out_val, ctx.eflags);
        break;
    case InstructionId::Xor:
        out_val = a ^ b;
        new_flags = CalculateLogicFlags(out_val, ctx.eflags);
        break;
    case InstructionId::Test:
        out_val = a & b;
        new_flags = CalculateLogicFlags(out_val, ctx.eflags);
        write_back = false;
        break;
    default:
        return Error{ErrorCode::InvalidArgument, "Instrução ALU/Lógica desconhecida"};
    }

    if (write_back) {
        auto w_res = inst.op1.Write(ctx, mem, out_val);
        if (!w_res)
            return w_res.error();
    }

    ctx.SetEflags(new_flags);
    return res;
}

} // namespace detail

// Template instantiations
template Result<ExecutionResult>
detail::ExecuteAlu<memory::AddressSpace>(const DecodedInstruction& inst, CpuContext& ctx,
                                         memory::AddressSpace& mem);

template Result<ExecutionResult>
detail::ExecuteAlu<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                          memory::VirtualMemory& mem);

} // namespace xblob::cpu
