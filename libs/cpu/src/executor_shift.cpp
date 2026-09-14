#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteShift(const DecodedInstruction& inst, CpuContext& ctx,
                                     MemoryType& mem) {
    auto val_res = inst.op1.Read(ctx, mem);
    if (!val_res) {
        return val_res.error();
    }
    auto count_res = inst.op2.Read(ctx, mem);
    if (!count_res) {
        return count_res.error();
    }

    const u32 raw_count = *count_res & 0x1FU;
    if (raw_count == 0) {
        // Count zero: target and flags remain untouched
        return ExecutionResult{};
    }

    const u8 size = inst.op1.size_bytes;
    u32 result = 0;
    bool cf = false;
    bool of = false;
    bool update_arith_flags = false;

    if (size == 1) {
        const u32 val = static_cast<u32>(*val_res & 0xFFU);
        switch (inst.id) {
        case InstructionId::Shl: {
            cf = (raw_count <= 8) ? (((val >> (8 - raw_count)) & 1) != 0) : false;
            const u8 res = (raw_count < 8) ? static_cast<u8>((val << raw_count) & 0xFFU) : 0U;
            result = res;
            if (raw_count == 1) {
                of = (((res >> 7) & 1) ^ (cf ? 1 : 0)) != 0;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Shr: {
            cf = (raw_count <= 8) ? (((val >> (raw_count - 1)) & 1) != 0) : false;
            const u8 res = (raw_count < 8) ? static_cast<u8>(val >> raw_count) : 0U;
            result = res;
            if (raw_count == 1) {
                of = ((val >> 7) & 1) != 0;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Sar: {
            const auto sval = static_cast<int8_t>(val);
            const auto sres = (raw_count < 8) ? static_cast<int8_t>(sval >> raw_count)
                                              : (sval < 0 ? static_cast<int8_t>(-1) : 0);
            cf = (raw_count <= 8) ? (((sval >> (raw_count - 1)) & 1) != 0) : (sval < 0);
            result = static_cast<u8>(sres);
            if (raw_count == 1) {
                of = false;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Rol: {
            const u32 cnt = raw_count % 8U;
            if (cnt == 0) {
                return ExecutionResult{};
            }
            const u8 res = static_cast<u8>(((val << cnt) | (val >> (8 - cnt))) & 0xFFU);
            result = res;
            cf = (res & 1) != 0;
            if (raw_count == 1) {
                of = (((res >> 7) & 1) ^ (cf ? 1 : 0)) != 0;
            }
            break;
        }
        case InstructionId::Ror: {
            const u32 cnt = raw_count % 8U;
            if (cnt == 0) {
                return ExecutionResult{};
            }
            const u8 res = static_cast<u8>(((val >> cnt) | (val << (8 - cnt))) & 0xFFU);
            result = res;
            cf = ((res >> 7) & 1) != 0;
            if (raw_count == 1) {
                of = (((res >> 7) & 1) ^ ((res >> 6) & 1)) != 0;
            }
            break;
        }
        default:
            return Error{ErrorCode::InvalidOpcode, "Operação shift desconhecida"};
        }
    } else {
        const u32 val = *val_res;
        switch (inst.id) {
        case InstructionId::Shl: {
            cf = ((val >> (32 - raw_count)) & 1) != 0;
            result = val << raw_count;
            if (raw_count == 1) {
                of = (((result >> 31) & 1) ^ (cf ? 1 : 0)) != 0;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Shr: {
            cf = ((val >> (raw_count - 1)) & 1) != 0;
            result = val >> raw_count;
            if (raw_count == 1) {
                of = ((val >> 31) & 1) != 0;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Sar: {
            const auto sval = static_cast<int32_t>(val);
            cf = ((sval >> (raw_count - 1)) & 1) != 0;
            result = static_cast<u32>(sval >> raw_count);
            if (raw_count == 1) {
                of = false;
            }
            update_arith_flags = true;
            break;
        }
        case InstructionId::Rol: {
            const u32 cnt = raw_count % 32U;
            if (cnt == 0) {
                return ExecutionResult{};
            }
            result = (val << cnt) | (val >> (32 - cnt));
            cf = (result & 1) != 0;
            if (raw_count == 1) {
                of = (((result >> 31) & 1) ^ (cf ? 1 : 0)) != 0;
            }
            break;
        }
        case InstructionId::Ror: {
            const u32 cnt = raw_count % 32U;
            if (cnt == 0) {
                return ExecutionResult{};
            }
            result = (val >> cnt) | (val << (32 - cnt));
            cf = ((result >> 31) & 1) != 0;
            if (raw_count == 1) {
                of = (((result >> 31) & 1) ^ ((result >> 30) & 1)) != 0;
            }
            break;
        }
        default:
            return Error{ErrorCode::InvalidOpcode, "Operação shift desconhecida"};
        }
    }

    auto write_res = inst.op1.Write(ctx, mem, result);
    if (!write_res) {
        return write_res.error();
    }

    ctx.SetFlag(kFlagCF, cf);
    if (raw_count == 1) {
        ctx.SetFlag(kFlagOF, of);
    }
    if (update_arith_flags) {
        if (size == 1) {
            ctx.SetFlag(kFlagSF, (result & 0x80U) != 0);
            ctx.SetFlag(kFlagZF, (result & 0xFFU) == 0);
            ctx.SetFlag(kFlagPF, CalculateParity(static_cast<u8>(result & 0xFFU)));
        } else {
            ctx.SetFlag(kFlagSF, (result & 0x80000000U) != 0);
            ctx.SetFlag(kFlagZF, result == 0);
            ctx.SetFlag(kFlagPF, CalculateParity(static_cast<u8>(result & 0xFFU)));
        }
    }

    return ExecutionResult{};
}

template Result<ExecutionResult> ExecuteShift<memory::AddressSpace>(const DecodedInstruction& inst,
                                                                    CpuContext& ctx,
                                                                    memory::AddressSpace& mem);
template Result<ExecutionResult> ExecuteShift<memory::VirtualMemory>(const DecodedInstruction& inst,
                                                                     CpuContext& ctx,
                                                                     memory::VirtualMemory& mem);

} // namespace xblob::cpu::detail
