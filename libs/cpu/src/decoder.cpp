#include "xblob/cpu/decoder.hpp"

#include "decoder_alu.hpp"
#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

using detail::InstructionStream;
using detail::ParseModRm;

template <typename MemoryType>
Result<DecodedInstruction> Decoder::Decode(const CpuContext& ctx, MemoryType& mem, GuestAddr eip) {
    InstructionStream<MemoryType> stream(mem, eip);

    auto opcode_res = stream.NextByte();
    if (!opcode_res) {
        return opcode_res.error();
    }
    const u8 opcode = *opcode_res;

    DecodedInstruction inst;

    // NOP
    if (opcode == opcodes::kNop) {
        inst.id = InstructionId::Nop;
        inst.cycles = cycles::kNop;
        inst.length = stream.length();
        return inst;
    }

    // HLT
    if (opcode == opcodes::kHlt) {
        inst.id = InstructionId::Hlt;
        inst.cycles = cycles::kHlt;
        inst.length = stream.length();
        return inst;
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
        return inst;
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
        return inst;
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
        return inst;
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
        return inst;
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
        return inst;
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
        return inst;
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
        return inst;
    }

    // PUSH r32 (0x50..0x57)
    if (opcode >= opcodes::kPushRegBase && opcode <= opcodes::kPushRegEnd) {
        inst.id = InstructionId::Push;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kPushRegBase));
        inst.cycles = cycles::kPushReg;
        inst.length = stream.length();
        return inst;
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
        return inst;
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
        return inst;
    }

    // POP r32 (0x58..0x5F)
    if (opcode >= opcodes::kPopRegBase && opcode <= opcodes::kPopRegEnd) {
        inst.id = InstructionId::Pop;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kPopRegBase));
        inst.cycles = cycles::kPopReg;
        inst.length = stream.length();
        return inst;
    }

    // POP r/m32 (0x8F /0)
    if (opcode == opcodes::kPopRm32) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        if (modrm_res->second != 0) {
            return Error{ErrorCode::InvalidOpcode, "Opcode 0x8F com reg != 0"};
        }
        inst.id = InstructionId::Pop;
        inst.op1 = modrm_res->first;
        inst.cycles = (inst.op1.kind == OperandKind::Memory) ? cycles::kPopMem : cycles::kPopReg;
        inst.length = stream.length();
        return inst;
    }

    // PUSHF (0x9C)
    if (opcode == opcodes::kPushf) {
        inst.id = InstructionId::Pushf;
        inst.cycles = cycles::kPushf;
        inst.length = stream.length();
        return inst;
    }

    // POPF (0x9D)
    if (opcode == opcodes::kPopf) {
        inst.id = InstructionId::Popf;
        inst.cycles = cycles::kPopf;
        inst.length = stream.length();
        return inst;
    }

    // CALL rel32 (0xE8)
    if (opcode == opcodes::kCallRel32) {
        auto disp_res = stream.ReadImm32();
        if (!disp_res)
            return disp_res.error();
        inst.id = InstructionId::Call;
        inst.length = stream.length();
        inst.branch_target = (eip + inst.length) + *disp_res;
        inst.op1 = Operand::MakeImm(inst.branch_target);
        inst.cycles = cycles::kCallRel32;
        return inst;
    }

    // RET near (0xC3)
    if (opcode == opcodes::kRetNear) {
        inst.id = InstructionId::Ret;
        inst.ret_pop_bytes = 0;
        inst.cycles = cycles::kRet;
        inst.length = stream.length();
        return inst;
    }

    // RET near imm16 (0xC2)
    if (opcode == opcodes::kRetNearImm16) {
        auto imm_res = stream.ReadImm16();
        if (!imm_res)
            return imm_res.error();
        inst.id = InstructionId::Ret;
        inst.ret_pop_bytes = *imm_res;
        inst.cycles = cycles::kRet;
        inst.length = stream.length();
        return inst;
    }

    // JMP rel8 (0xEB)
    if (opcode == opcodes::kJmpRel8) {
        auto disp_res = stream.ReadImm8();
        if (!disp_res)
            return disp_res.error();
        inst.id = InstructionId::Jmp;
        inst.length = stream.length();
        inst.branch_target =
            (eip + inst.length) + static_cast<u32>(static_cast<int32_t>(*disp_res));
        inst.op1 = Operand::MakeImm(inst.branch_target);
        inst.cycles = cycles::kJmpRel8;
        return inst;
    }

    // JMP rel32 (0xE9)
    if (opcode == opcodes::kJmpRel32) {
        auto disp_res = stream.ReadImm32();
        if (!disp_res)
            return disp_res.error();
        inst.id = InstructionId::Jmp;
        inst.length = stream.length();
        inst.branch_target = (eip + inst.length) + *disp_res;
        inst.op1 = Operand::MakeImm(inst.branch_target);
        inst.cycles = cycles::kJmpRel32;
        return inst;
    }

    // Group 5 (0xFF): INC /0, DEC /1, CALL /2, JMP /4, PUSH /6
    if (opcode == opcodes::kGroup5) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        inst.op1 = modrm_res->first;
        u8 reg = modrm_res->second;
        if (reg == 0) {
            inst.id = InstructionId::Inc;
            inst.cycles =
                (inst.op1.kind == OperandKind::Memory) ? cycles::kAluMem : cycles::kAluRegReg;
        } else if (reg == 1) {
            inst.id = InstructionId::Dec;
            inst.cycles =
                (inst.op1.kind == OperandKind::Memory) ? cycles::kAluMem : cycles::kAluRegReg;
        } else if (reg == 2) {
            inst.id = InstructionId::Call;
            inst.cycles = cycles::kCallRm;
        } else if (reg == 4) {
            inst.id = InstructionId::Jmp;
            inst.cycles = cycles::kJmpRm;
        } else if (reg == 6) {
            inst.id = InstructionId::Push;
            inst.cycles =
                (inst.op1.kind == OperandKind::Memory) ? cycles::kPushMem : cycles::kPushReg;
        } else {
            return Error{ErrorCode::InvalidOpcode, "Opcode 0xFF com reg não suportado"};
        }
        inst.length = stream.length();
        return inst;
    }

    // INC r32 (0x40..0x47)
    if (opcode >= opcodes::kIncRegBase && opcode <= opcodes::kIncRegEnd) {
        inst.id = InstructionId::Inc;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kIncRegBase));
        inst.cycles = cycles::kAluRegReg;
        inst.length = stream.length();
        return inst;
    }

    // DEC r32 (0x48..0x4F)
    if (opcode >= opcodes::kDecRegBase && opcode <= opcodes::kDecRegEnd) {
        inst.id = InstructionId::Dec;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kDecRegBase));
        inst.cycles = cycles::kAluRegReg;
        inst.length = stream.length();
        return inst;
    }

    // ALU instructions (ADD, ADC, SUB, SBB, CMP, AND, OR, XOR, TEST, Group 1, Group 3)
    auto alu_res = detail::TryDecodeAlu(opcode, stream, ctx);
    if (!alu_res)
        return alu_res.error();
    if (*alu_res)
        return **alu_res;

    // Jcc rel8 (0x70..0x7F)
    if (opcode >= opcodes::kJccRel8Base && opcode <= opcodes::kJccRel8End) {
        auto disp_res = stream.ReadImm8();
        if (!disp_res)
            return disp_res.error();
        inst.id = InstructionId::Jcc;
        inst.condition = static_cast<ConditionCode>(opcode - opcodes::kJccRel8Base);
        inst.length = stream.length();
        inst.branch_target =
            (eip + inst.length) + static_cast<u32>(static_cast<int32_t>(*disp_res));
        inst.cycles = cycles::kJccTaken;
        return inst;
    }

    // Two-byte escape (0x0F)
    if (opcode == opcodes::kTwoByteEscape) {
        auto sec_res = stream.NextByte();
        if (!sec_res)
            return sec_res.error();
        u8 sec_opcode = *sec_res;

        // Jcc rel32 (0x0F 0x80..0x8F)
        if (sec_opcode >= opcodes::kJccRel32Base && sec_opcode <= opcodes::kJccRel32End) {
            auto disp_res = stream.ReadImm32();
            if (!disp_res)
                return disp_res.error();
            inst.id = InstructionId::Jcc;
            inst.condition = static_cast<ConditionCode>(sec_opcode - opcodes::kJccRel32Base);
            inst.length = stream.length();
            inst.branch_target = (eip + inst.length) + *disp_res;
            inst.cycles = cycles::kJccTaken;
            return inst;
        }

        return Error{ErrorCode::InvalidOpcode, "Prefixo 0x0F com opcode não suportado"};
    }

    // CLI (0xFA)
    if (opcode == opcodes::kCli) {
        inst.id = InstructionId::Cli;
        inst.cycles = cycles::kCli;
        inst.length = stream.length();
        return inst;
    }

    // STI (0xFB)
    if (opcode == opcodes::kSti) {
        inst.id = InstructionId::Sti;
        inst.cycles = cycles::kSti;
        inst.length = stream.length();
        return inst;
    }

    // IRET (0xCF)
    if (opcode == opcodes::kIret) {
        inst.id = InstructionId::Iret;
        inst.cycles = cycles::kIret;
        inst.length = stream.length();
        return inst;
    }

    // INT imm8 (0xCD)
    if (opcode == opcodes::kIntImm8) {
        auto vec_res = stream.NextByte();
        if (!vec_res)
            return vec_res.error();
        inst.id = InstructionId::Int;
        inst.int_vector = *vec_res;
        inst.cycles = cycles::kInt;
        inst.length = stream.length();
        return inst;
    }

    return Error{ErrorCode::InvalidOpcode, "Opcode não suportado no subconjunto IA-32", opcode};
}

// Explicit template instantiations
template Result<DecodedInstruction> Decoder::Decode<memory::AddressSpace>(const CpuContext& ctx,
                                                                          memory::AddressSpace& mem,
                                                                          GuestAddr eip);

template Result<DecodedInstruction>
Decoder::Decode<memory::VirtualMemory>(const CpuContext& ctx, memory::VirtualMemory& mem,
                                       GuestAddr eip);

} // namespace xblob::cpu
