#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeBitExt(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
                std::optional<UnsupportedFormInfo>* out_unsupported) {
    if (opcode == opcodes::kCdq) {
        DecodedInstruction inst;
        inst.id = InstructionId::Cdq;
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }
    (void)ctx;
    (void)out_unsupported;
    return std::optional<DecodedInstruction>(std::nullopt);
}

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeTwoByteExtended(u8 second_byte, InstructionStream<MemoryType>& stream,
                         const CpuContext& ctx,
                         std::optional<UnsupportedFormInfo>* out_unsupported) {
    // MOVZX r32, r/m8 (0x0F 0xB6) and MOVZX r32, r/m16 (0x0F 0xB7)
    if (second_byte == opcodes::kMovzxRm8 || second_byte == opcodes::kMovzxRm16) {
        const u8 src_size = (second_byte == opcodes::kMovzxRm8) ? 1 : 2;
        auto modrm_res = ParseModRm(stream, ctx, src_size);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Movzx;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.op2 = modrm_res->first;
        inst.data_size = src_size;
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOVSX r32, r/m8 (0x0F 0xBE) and MOVSX r32, r/m16 (0x0F 0xBF)
    if (second_byte == opcodes::kMovsxRm8 || second_byte == opcodes::kMovsxRm16) {
        const u8 src_size = (second_byte == opcodes::kMovsxRm8) ? 1 : 2;
        auto modrm_res = ParseModRm(stream, ctx, src_size);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Movsx;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.op2 = modrm_res->first;
        inst.data_size = src_size;
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // BT r/m32, r32 (0x0F 0xA3)
    if (second_byte == opcodes::kBtRmReg) {
        auto modrm_res = ParseModRm(stream, ctx, 4);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Bt;
        inst.op1 = modrm_res->first;
        inst.op2 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // BT r/m32, imm8 (0x0F 0xBA /4)
    if (second_byte == opcodes::kBtGroup8) {
        auto modrm_res = ParseModRm(stream, ctx, 4);
        if (!modrm_res) {
            return modrm_res.error();
        }
        if (modrm_res->second != 4) {
            std::string reason =
                "0x0F 0xBA group 8 unsupported reg=" + std::to_string(modrm_res->second);
            if (out_unsupported != nullptr) {
                *out_unsupported =
                    UnsupportedFormInfo{stream.start_eip(), stream.raw_bytes(), reason};
            }
            return Error{ErrorCode::UnsupportedFeature, reason, stream.start_eip()};
        }
        auto imm_res = stream.ReadImm8();
        if (!imm_res) {
            return imm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Bt;
        inst.op1 = modrm_res->first;
        inst.op2 = Operand::MakeImm(static_cast<u8>(*imm_res), 1);
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // SETcc r/m8 (0x0F 0x90 .. 0x0F 0x9F)
    if (second_byte >= opcodes::kSetccBase && second_byte <= opcodes::kSetccEnd) {
        auto modrm_res = ParseModRm(stream, ctx, 1);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Setcc;
        inst.op1 = modrm_res->first;
        inst.condition = static_cast<ConditionCode>(second_byte & 0x0F);
        inst.cycles = cycles::kBitExt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // IMUL r32, r/m32 (0x0F 0xAF)
    if (second_byte == opcodes::kImulRegRm32) {
        auto modrm_res = ParseModRm(stream, ctx, 4);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Imul;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second), 4);
        inst.op2 = modrm_res->first;
        inst.cycles = cycles::kMul;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // CMPXCHG r/m8, r8 (0x0F 0xB0) and CMPXCHG r/m32, r32 (0x0F 0xB1)
    if (second_byte == opcodes::kCmpxchgRmReg8 || second_byte == opcodes::kCmpxchgRmReg32) {
        const bool is_8bit = (second_byte == opcodes::kCmpxchgRmReg8);
        const u8 size_bytes = is_8bit ? 1 : 4;
        auto modrm_res = ParseModRm(stream, ctx, size_bytes);
        if (!modrm_res) {
            return modrm_res.error();
        }
        DecodedInstruction inst;
        inst.id = InstructionId::Cmpxchg;
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
