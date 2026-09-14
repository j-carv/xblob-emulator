#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeAtomic(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
                std::optional<UnsupportedFormInfo>* out_unsupported) {
    (void)out_unsupported;

    // XCHG EAX, r32 (0x91..0x97). Note 0x90 is NOP.
    if (opcode > opcodes::kXchgEaxRegBase && opcode <= opcodes::kXchgEaxRegEnd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Xchg;
        inst.op1 = Operand::MakeReg(Reg32::EAX, 4);
        inst.op2 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kXchgEaxRegBase), 4);
        inst.data_size = 4;
        inst.cycles = cycles::kAtomic;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // XCHG r/m8, r8 (0x86) and XCHG r/m32, r32 (0x87)
    if (opcode == opcodes::kXchgRmReg8 || opcode == opcodes::kXchgRmReg32) {
        const bool is_8bit = (opcode == opcodes::kXchgRmReg8);
        const u8 size_bytes = is_8bit ? 1 : 4;
        auto modrm_res = ParseModRm(stream, ctx, size_bytes);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Xchg;
        inst.op1 = modrm_res->first;
        inst.op2 = is_8bit ? Operand::MakeReg8(modrm_res->second)
                           : Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.data_size = size_bytes;
        inst.cycles = cycles::kAtomic;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    return std::optional<DecodedInstruction>(std::nullopt);
}

} // namespace xblob::cpu::detail
