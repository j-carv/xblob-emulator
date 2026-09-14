#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeAlu(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
             std::optional<UnsupportedFormInfo>* out_unsupported = nullptr) {
    DecodedInstruction inst;

    auto parse_two_op = [&](InstructionId id, bool is_rm_reg,
                            bool is_eax_imm) -> Result<std::optional<DecodedInstruction>> {
        inst.id = id;
        if (is_eax_imm) {
            auto imm_res = stream.ReadImm32();
            if (!imm_res)
                return imm_res.error();
            inst.op1 = Operand::MakeReg(Reg32::EAX);
            inst.op2 = Operand::MakeImm(*imm_res);
            inst.cycles = cycles::kAluRegImm;
        } else {
            auto modrm_res = ParseModRm(stream, ctx);
            if (!modrm_res)
                return modrm_res.error();
            if (is_rm_reg) {
                inst.op1 = modrm_res->first;
                inst.op2 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second));
            } else {
                inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second));
                inst.op2 = modrm_res->first;
            }
            inst.cycles =
                (inst.op1.kind == OperandKind::Memory || inst.op2.kind == OperandKind::Memory)
                    ? cycles::kAluMem
                    : cycles::kAluRegReg;
        }
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    };

    // ADD (0x01, 0x03, 0x05)
    if (opcode == opcodes::kAddRmReg32)
        return parse_two_op(InstructionId::Add, true, false);
    if (opcode == opcodes::kAddRegRm32)
        return parse_two_op(InstructionId::Add, false, false);
    if (opcode == opcodes::kAddEaxImm32)
        return parse_two_op(InstructionId::Add, false, true);

    // ADC (0x11, 0x13, 0x15)
    if (opcode == opcodes::kAdcRmReg32)
        return parse_two_op(InstructionId::Adc, true, false);
    if (opcode == opcodes::kAdcRegRm32)
        return parse_two_op(InstructionId::Adc, false, false);
    if (opcode == opcodes::kAdcEaxImm32)
        return parse_two_op(InstructionId::Adc, false, true);

    // SUB (0x29, 0x2B, 0x2D)
    if (opcode == opcodes::kSubRmReg32)
        return parse_two_op(InstructionId::Sub, true, false);
    if (opcode == opcodes::kSubRegRm32)
        return parse_two_op(InstructionId::Sub, false, false);
    if (opcode == opcodes::kSubEaxImm32)
        return parse_two_op(InstructionId::Sub, false, true);

    // SBB (0x19, 0x1B, 0x1D)
    if (opcode == opcodes::kSbbRmReg32)
        return parse_two_op(InstructionId::Sbb, true, false);
    if (opcode == opcodes::kSbbRegRm32)
        return parse_two_op(InstructionId::Sbb, false, false);
    if (opcode == opcodes::kSbbEaxImm32)
        return parse_two_op(InstructionId::Sbb, false, true);

    // CMP (0x39, 0x3B, 0x3D)
    if (opcode == opcodes::kCmpRmReg32)
        return parse_two_op(InstructionId::Cmp, true, false);
    if (opcode == opcodes::kCmpRegRm32)
        return parse_two_op(InstructionId::Cmp, false, false);
    if (opcode == opcodes::kCmpEaxImm32)
        return parse_two_op(InstructionId::Cmp, false, true);

    // AND (0x21, 0x23, 0x25)
    if (opcode == opcodes::kAndRmReg32)
        return parse_two_op(InstructionId::And, true, false);
    if (opcode == opcodes::kAndRegRm32)
        return parse_two_op(InstructionId::And, false, false);
    if (opcode == opcodes::kAndEaxImm32)
        return parse_two_op(InstructionId::And, false, true);

    // OR (0x09, 0x0B, 0x0D)
    if (opcode == opcodes::kOrRmReg32)
        return parse_two_op(InstructionId::Or, true, false);
    if (opcode == opcodes::kOrRegRm32)
        return parse_two_op(InstructionId::Or, false, false);
    if (opcode == opcodes::kOrEaxImm32)
        return parse_two_op(InstructionId::Or, false, true);

    // XOR (0x31, 0x33, 0x35)
    if (opcode == opcodes::kXorRmReg32)
        return parse_two_op(InstructionId::Xor, true, false);
    if (opcode == opcodes::kXorRegRm32)
        return parse_two_op(InstructionId::Xor, false, false);
    if (opcode == opcodes::kXorEaxImm32)
        return parse_two_op(InstructionId::Xor, false, true);

    // TEST (0x85, 0xA9)
    if (opcode == opcodes::kTestRmReg32)
        return parse_two_op(InstructionId::Test, true, false);
    if (opcode == opcodes::kTestEaxImm32)
        return parse_two_op(InstructionId::Test, false, true);

    // Group 3 TEST, MUL, IMUL, DIV, IDIV (0xF7 / 0xF6)
    if (opcode == opcodes::kGroup3Rm32 || opcode == opcodes::kGroup3Rm8) {
        const bool is_8bit = (opcode == opcodes::kGroup3Rm8);
        const u8 size_bytes = is_8bit ? 1 : 4;
        auto modrm_res = ParseModRm(stream, ctx, size_bytes);
        if (!modrm_res)
            return modrm_res.error();
        const u8 reg_op = modrm_res->second;
        inst.op1 = modrm_res->first;
        inst.data_size = size_bytes;

        if (reg_op == 0) { // TEST r/m, imm
            if (is_8bit) {
                auto imm_res = stream.NextByte();
                if (!imm_res)
                    return imm_res.error();
                inst.id = InstructionId::Test;
                inst.op2 = Operand::MakeImm(*imm_res, 1);
            } else {
                auto imm_res = stream.ReadImm32();
                if (!imm_res)
                    return imm_res.error();
                inst.id = InstructionId::Test;
                inst.op2 = Operand::MakeImm(*imm_res, 4);
            }
            inst.cycles =
                (inst.op1.kind == OperandKind::Memory) ? cycles::kAluMem : cycles::kAluRegImm;
            inst.length = stream.length();
            return std::optional<DecodedInstruction>(inst);
        }
        if (reg_op == 4) {
            inst.id = InstructionId::Mul;
            inst.cycles = cycles::kMul;
            inst.length = stream.length();
            return std::optional<DecodedInstruction>(inst);
        }
        if (reg_op == 5) {
            inst.id = InstructionId::Imul;
            inst.cycles = cycles::kMul;
            inst.length = stream.length();
            return std::optional<DecodedInstruction>(inst);
        }
        if (reg_op == 6) {
            inst.id = InstructionId::Div;
            inst.cycles = cycles::kDiv;
            inst.length = stream.length();
            return std::optional<DecodedInstruction>(inst);
        }
        if (reg_op == 7) {
            inst.id = InstructionId::Idiv;
            inst.cycles = cycles::kDiv;
            inst.length = stream.length();
            return std::optional<DecodedInstruction>(inst);
        }

        std::string reason = "Group 3 opcode 0xF7/F6 unsupported reg=" + std::to_string(reg_op);
        if (out_unsupported != nullptr) {
            *out_unsupported = UnsupportedFormInfo{stream.start_eip(), stream.raw_bytes(), reason};
        }
        return Error{ErrorCode::UnsupportedFeature, reason, stream.start_eip()};
    }

    // Group 1: 0x81 (imm32) and 0x83 (imm8 sign-extended)
    if (opcode == opcodes::kGroup1Imm32 || opcode == opcodes::kGroup1Imm8) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        u32 imm_val = 0;
        if (opcode == opcodes::kGroup1Imm32) {
            auto imm_res = stream.ReadImm32();
            if (!imm_res)
                return imm_res.error();
            imm_val = *imm_res;
        } else {
            auto imm_res = stream.ReadImm8();
            if (!imm_res)
                return imm_res.error();
            imm_val = static_cast<u32>(static_cast<int32_t>(*imm_res));
        }

        inst.op1 = modrm_res->first;
        inst.op2 = Operand::MakeImm(imm_val);
        inst.cycles = (inst.op1.kind == OperandKind::Memory) ? cycles::kAluMem : cycles::kAluRegImm;

        switch (modrm_res->second) {
        case 0:
            inst.id = InstructionId::Add;
            break;
        case 1:
            inst.id = InstructionId::Or;
            break;
        case 2:
            inst.id = InstructionId::Adc;
            break;
        case 3:
            inst.id = InstructionId::Sbb;
            break;
        case 4:
            inst.id = InstructionId::And;
            break;
        case 5:
            inst.id = InstructionId::Sub;
            break;
        case 6:
            inst.id = InstructionId::Xor;
            break;
        case 7:
            inst.id = InstructionId::Cmp;
            break;
        default:
            return Error{ErrorCode::InvalidOpcode, "Opcode 0x81/0x83 reg inválido"};
        }
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    return std::optional<DecodedInstruction>(std::nullopt);
}

} // namespace xblob::cpu::detail
