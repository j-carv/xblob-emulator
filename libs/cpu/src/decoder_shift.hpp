#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeShift(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
               std::optional<UnsupportedFormInfo>* out_unsupported) {
    bool is_group2 = (opcode == opcodes::kGroup2Imm8 || opcode == opcodes::kGroup2Imm8_8bit ||
                      opcode == opcodes::kGroup2One || opcode == opcodes::kGroup2One_8bit ||
                      opcode == opcodes::kGroup2Cl || opcode == opcodes::kGroup2Cl_8bit);
    if (!is_group2) {
        return std::optional<DecodedInstruction>(std::nullopt);
    }

    const bool is_8bit = (opcode == opcodes::kGroup2Imm8_8bit ||
                          opcode == opcodes::kGroup2One_8bit || opcode == opcodes::kGroup2Cl_8bit);
    const u8 size_bytes = is_8bit ? 1 : 4;

    auto modrm_res = ParseModRm(stream, ctx, size_bytes);
    if (!modrm_res) {
        return modrm_res.error();
    }

    DecodedInstruction inst;
    inst.op1 = modrm_res->first;
    const u8 reg_op = modrm_res->second;

    switch (reg_op) {
    case 0:
        inst.id = InstructionId::Rol;
        break;
    case 1:
        inst.id = InstructionId::Ror;
        break;
    case 4:
    case 6:
        inst.id = InstructionId::Shl;
        break;
    case 5:
        inst.id = InstructionId::Shr;
        break;
    case 7:
        inst.id = InstructionId::Sar;
        break;
    case 2:
    case 3:
    default: {
        std::string reason = (reg_op == 2)   ? "RCL rotation with carry is unsupported"
                             : (reg_op == 3) ? "RCR rotation with carry is unsupported"
                                             : "Shift group 2 reserved form unsupported";
        if (out_unsupported != nullptr) {
            *out_unsupported = UnsupportedFormInfo{
                stream.start_eip(),
                stream.raw_bytes(),
                reason,
            };
        }
        return Error{ErrorCode::UnsupportedFeature, reason, stream.start_eip()};
    }
    }

    if (opcode == opcodes::kGroup2One || opcode == opcodes::kGroup2One_8bit) {
        inst.op2 = Operand::MakeImm(1, 1);
    } else if (opcode == opcodes::kGroup2Cl || opcode == opcodes::kGroup2Cl_8bit) {
        inst.op2 = Operand::MakeReg8(1); // CL
    } else {
        auto imm_res = stream.NextByte();
        if (!imm_res) {
            return imm_res.error();
        }
        inst.op2 = Operand::MakeImm(*imm_res, 1);
    }

    inst.cycles = (inst.op1.kind == OperandKind::Memory) ? cycles::kShiftMem : cycles::kShiftReg;
    inst.length = stream.length();
    return std::optional<DecodedInstruction>(inst);
}

} // namespace xblob::cpu::detail
