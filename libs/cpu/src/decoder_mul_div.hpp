#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeMulDiv(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
                std::optional<UnsupportedFormInfo>* out_unsupported) {
    (void)out_unsupported;
    if (opcode == opcodes::kImulRegRmImm32 || opcode == opcodes::kImulRegRmImm8) {
        auto modrm_res = ParseModRm(stream, ctx, 4);
        if (!modrm_res) {
            return modrm_res.error();
        }

        DecodedInstruction inst;
        inst.id = InstructionId::Imul;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.op2 = modrm_res->first;

        if (opcode == opcodes::kImulRegRmImm32) {
            auto imm_res = stream.ReadImm32();
            if (!imm_res) {
                return imm_res.error();
            }
            inst.op3 = Operand::MakeImm(*imm_res, 4);
        } else {
            auto imm_res = stream.ReadImm8();
            if (!imm_res) {
                return imm_res.error();
            }
            inst.op3 = Operand::MakeImm(static_cast<u32>(static_cast<int32_t>(*imm_res)), 4);
        }

        inst.cycles = cycles::kMul;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    return std::optional<DecodedInstruction>(std::nullopt);
}

} // namespace xblob::cpu::detail
