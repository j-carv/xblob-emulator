#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeData(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx) {
    DecodedInstruction inst;

    // NOP
    if (opcode == opcodes::kNop) {
        inst.id = InstructionId::Nop;
        inst.cycles = cycles::kNop;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // HLT
    if (opcode == opcodes::kHlt) {
        inst.id = InstructionId::Hlt;
        inst.cycles = cycles::kHlt;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV r32, imm32 (0xB8..0xBF)
    if (opcode >= opcodes::kMovRegImmBase && opcode <= opcodes::kMovRegImmEnd) {
        auto imm_res = stream.ReadImm32();
        if (!imm_res)
            return imm_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kMovRegImmBase));
        inst.op2 = Operand::MakeImm(*imm_res);
        inst.cycles = cycles::kMovRegImm;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV r/m32, r32 (0x89)
    if (opcode == opcodes::kMovRmReg32) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = modrm_res->first;
        inst.op2 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second));
        inst.cycles = (inst.op1.kind == OperandKind::Memory) ? cycles::kMovMem : cycles::kMovRegReg;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV r32, r/m32 (0x8B)
    if (opcode == opcodes::kMovRegRm32) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second));
        inst.op2 = modrm_res->first;
        inst.cycles = (inst.op2.kind == OperandKind::Memory) ? cycles::kMovMem : cycles::kMovRegReg;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV r/m32, imm32 (0xC7 /0)
    if (opcode == opcodes::kMovRmImm32) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        if (modrm_res->second != 0) {
            return Error{ErrorCode::InvalidOpcode, "Opcode 0xC7 com reg != 0"};
        }
        auto imm_res = stream.ReadImm32();
        if (!imm_res)
            return imm_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = modrm_res->first;
        inst.op2 = Operand::MakeImm(*imm_res);
        inst.cycles = (inst.op1.kind == OperandKind::Memory) ? cycles::kMovMem : cycles::kMovRegImm;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV EAX, moffs32 (0xA1)
    if (opcode == opcodes::kMovEaxMoffs32) {
        auto addr_res = stream.ReadImm32();
        if (!addr_res)
            return addr_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = Operand::MakeReg(Reg32::EAX);
        inst.op2 = Operand::MakeMem(*addr_res, 4);
        inst.cycles = cycles::kMovEaxMoffs32;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // MOV moffs32, EAX (0xA3)
    if (opcode == opcodes::kMovMoffs32Eax) {
        auto addr_res = stream.ReadImm32();
        if (!addr_res)
            return addr_res.error();
        inst.id = InstructionId::Mov;
        inst.op1 = Operand::MakeMem(*addr_res, 4);
        inst.op2 = Operand::MakeReg(Reg32::EAX);
        inst.cycles = cycles::kMovMoffs32Eax;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // LEA r32, m (0x8D)
    if (opcode == opcodes::kLea) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        if (modrm_res->first.kind != OperandKind::Memory) {
            return Error{ErrorCode::InvalidOpcode, "LEA requer operando de memória"};
        }
        inst.id = InstructionId::Lea;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(modrm_res->second));
        inst.op2 = Operand::MakeImm(modrm_res->first.mem_addr);
        inst.cycles = cycles::kLea;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // PUSH r32 (0x50..0x57)
    if (opcode >= opcodes::kPushRegBase && opcode <= opcodes::kPushRegEnd) {
        inst.id = InstructionId::Push;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kPushRegBase));
        inst.cycles = cycles::kPushReg;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // PUSH imm32 (0x68)
    if (opcode == opcodes::kPushImm32) {
        auto imm_res = stream.ReadImm32();
        if (!imm_res)
            return imm_res.error();
        inst.id = InstructionId::Push;
        inst.op1 = Operand::MakeImm(*imm_res);
        inst.cycles = cycles::kPushImm;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // PUSH imm8 (0x6A)
    if (opcode == opcodes::kPushImm8) {
        auto imm_res = stream.ReadImm8();
        if (!imm_res)
            return imm_res.error();
        inst.id = InstructionId::Push;
        inst.op1 = Operand::MakeImm(static_cast<u32>(static_cast<int32_t>(*imm_res)));
        inst.cycles = cycles::kPushImm;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // POP r32 (0x58..0x5F)
    if (opcode >= opcodes::kPopRegBase && opcode <= opcodes::kPopRegEnd) {
        inst.id = InstructionId::Pop;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kPopRegBase));
        inst.cycles = cycles::kPopReg;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // PUSHFD (0x9C)
    if (opcode == opcodes::kPushf) {
        inst.id = InstructionId::Pushf;
        inst.cycles = cycles::kPushf;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    // POPFD (0x9D)
    if (opcode == opcodes::kPopf) {
        inst.id = InstructionId::Popf;
        inst.cycles = cycles::kPopf;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    return std::optional<DecodedInstruction>(std::nullopt);
}

} // namespace xblob::cpu::detail
