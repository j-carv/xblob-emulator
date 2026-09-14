#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/cpu/instruction.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteBitExt(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem) {
    switch (inst.id) {
    case InstructionId::Movzx: {
        auto src_res = inst.op2.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        u32 extended = 0;
        if (inst.data_size == 1) {
            extended = *src_res & 0xFFU;
        } else {
            extended = *src_res & 0xFFFFU;
        }
        auto write_res = inst.op1.Write(ctx, mem, extended);
        if (!write_res) {
            return write_res.error();
        }
        return ExecutionResult{};
    }
    case InstructionId::Movsx: {
        auto src_res = inst.op2.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        u32 extended = 0;
        if (inst.data_size == 1) {
            extended =
                static_cast<u32>(static_cast<int32_t>(static_cast<int8_t>(*src_res & 0xFFU)));
        } else {
            extended =
                static_cast<u32>(static_cast<int32_t>(static_cast<int16_t>(*src_res & 0xFFFFU)));
        }
        auto write_res = inst.op1.Write(ctx, mem, extended);
        if (!write_res) {
            return write_res.error();
        }
        return ExecutionResult{};
    }
    case InstructionId::Cdq: {
        const u32 eax = ctx.GetGpr(Reg32::EAX);
        const u32 edx = ((eax & 0x80000000U) != 0) ? 0xFFFFFFFFU : 0U;
        ctx.SetGpr(Reg32::EDX, edx);
        return ExecutionResult{};
    }
    case InstructionId::Bt: {
        auto count_res = inst.op2.Read(ctx, mem);
        if (!count_res) {
            return count_res.error();
        }
        bool bit_val = false;
        if (inst.op1.kind == OperandKind::Register) {
            auto reg_val_res = inst.op1.Read(ctx, mem);
            if (!reg_val_res) {
                return reg_val_res.error();
            }
            const u32 bit_idx = *count_res % 32U;
            bit_val = ((*reg_val_res >> bit_idx) & 1U) != 0;
        } else {
            const auto bit_offset = static_cast<int32_t>(*count_res);
            // Effective address adjustment for bit indexing outside [0, 31]
            const int32_t word_offset = bit_offset >> 5;
            const u32 bit_idx = static_cast<u32>(bit_offset & 31);
            const GuestAddr word_ea = inst.op1.mem_addr + static_cast<u32>(word_offset * 4);
            auto word_res = mem.Read32(word_ea);
            if (!word_res) {
                return word_res.error();
            }
            bit_val = ((*word_res >> bit_idx) & 1U) != 0;
        }
        ctx.SetFlag(kFlagCF, bit_val);
        return ExecutionResult{};
    }
    case InstructionId::Setcc: {
        const bool condition_met = EvaluateCondition(inst.condition, ctx.eflags);
        const u32 byte_val = condition_met ? 1U : 0U;
        auto write_res = inst.op1.Write(ctx, mem, byte_val);
        if (!write_res) {
            return write_res.error();
        }
        return ExecutionResult{};
    }
    default:
        return Error{ErrorCode::InvalidOpcode, "Operação de bit/extensão desconhecida"};
    }
}

template Result<ExecutionResult> ExecuteBitExt<memory::AddressSpace>(const DecodedInstruction& inst,
                                                                     CpuContext& ctx,
                                                                     memory::AddressSpace& mem);
template Result<ExecutionResult>
ExecuteBitExt<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                     memory::VirtualMemory& mem);

} // namespace xblob::cpu::detail
