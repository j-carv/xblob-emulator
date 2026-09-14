#include "xblob/cpu/decoder.hpp"

#include "decoder_alu.hpp"
#include "decoder_atomic.hpp"
#include "decoder_bit_ext.hpp"
#include "decoder_data.hpp"
#include "decoder_mul_div.hpp"
#include "decoder_shift.hpp"
#include "decoder_string.hpp"
#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

using detail::InstructionStream;
using detail::ParseModRm;

template <typename MemoryType>
Result<DecodedInstruction> Decoder::Decode(const CpuContext& ctx, MemoryType& mem, GuestAddr eip,
                                           std::optional<UnsupportedFormInfo>* out_unsupported) {
    InstructionStream<MemoryType> stream(mem, eip);

    bool rep_prefix = false;
    bool repne_prefix = false;
    bool lock_prefix = false;
    bool op_size_prefix = false;
    bool addr_size_prefix = false;

    u8 opcode = 0;
    while (true) {
        auto next_byte = stream.NextByte();
        if (!next_byte) {
            return next_byte.error();
        }
        const u8 b = *next_byte;
        if (b == opcodes::kPrefixRep) {
            rep_prefix = true;
        } else if (b == opcodes::kPrefixRepne) {
            repne_prefix = true;
        } else if (b == opcodes::kPrefixLock) {
            lock_prefix = true;
        } else if (b == opcodes::kPrefixOpSize) {
            op_size_prefix = true;
        } else if (b == opcodes::kPrefixAddrSize) {
            addr_size_prefix = true;
        } else if (b == opcodes::kPrefixCs || b == opcodes::kPrefixSs || b == opcodes::kPrefixDs ||
                   b == opcodes::kPrefixEs || b == opcodes::kPrefixFs || b == opcodes::kPrefixGs) {
            // Segment override prefix accepted
        } else {
            opcode = b;
            break;
        }
    }

    // Prefix conflict checking
    if (rep_prefix && repne_prefix) {
        std::string reason = "Conflicting repeat prefixes (0xF2 and 0xF3 both present)";
        if (out_unsupported != nullptr) {
            *out_unsupported = UnsupportedFormInfo{eip, stream.raw_bytes(), reason};
        }
        return Error{ErrorCode::InvalidOpcode, reason, eip};
    }
    if (addr_size_prefix) {
        std::string reason = "16-bit address-size override prefix (0x67) is unsupported";
        if (out_unsupported != nullptr) {
            *out_unsupported = UnsupportedFormInfo{eip, stream.raw_bytes(), reason};
        }
        return Error{ErrorCode::UnsupportedFeature, reason, eip};
    }
    if (op_size_prefix) {
        std::string reason = "16-bit operand-size override prefix (0x66) is unsupported";
        if (out_unsupported != nullptr) {
            *out_unsupported = UnsupportedFormInfo{eip, stream.raw_bytes(), reason};
        }
        return Error{ErrorCode::UnsupportedFeature, reason, eip};
    }

    auto finalize = [&](DecodedInstruction inst) -> Result<DecodedInstruction> {
        inst.rep = rep_prefix;
        inst.repne = repne_prefix;
        inst.lock = lock_prefix;

        if ((inst.rep || inst.repne) && inst.id != InstructionId::Movs &&
            inst.id != InstructionId::Stos && inst.id != InstructionId::Lods &&
            inst.id != InstructionId::Cmps && inst.id != InstructionId::Scas) {
            std::string reason = "Repeat prefix on non-string instruction unsupported";
            if (out_unsupported != nullptr) {
                *out_unsupported = UnsupportedFormInfo{eip, stream.raw_bytes(), reason};
            }
            return Error{ErrorCode::UnsupportedFeature, reason, eip};
        }

        if (inst.lock && inst.id != InstructionId::Cmpxchg && inst.id != InstructionId::Xchg &&
            inst.id != InstructionId::Add && inst.id != InstructionId::Sub &&
            inst.id != InstructionId::And && inst.id != InstructionId::Or &&
            inst.id != InstructionId::Xor && inst.id != InstructionId::Inc &&
            inst.id != InstructionId::Dec) {
            std::string reason = "LOCK prefix invalid on specified instruction";
            if (out_unsupported != nullptr) {
                *out_unsupported = UnsupportedFormInfo{eip, stream.raw_bytes(), reason};
            }
            return Error{ErrorCode::InvalidOpcode, reason, eip};
        }

        return inst;
    };

    // 1. Shift & Rotate
    {
        auto shift_res = detail::TryDecodeShift(opcode, stream, ctx, out_unsupported);
        if (!shift_res)
            return shift_res.error();
        if (shift_res->has_value()) {
            return finalize(**shift_res);
        }
    }

    // 2. String ops & DF
    {
        auto string_res = detail::TryDecodeString(opcode, stream, ctx, out_unsupported);
        if (!string_res)
            return string_res.error();
        if (string_res->has_value()) {
            return finalize(**string_res);
        }
    }

    // 3. Multiply / Divide extra forms (IMUL 3-operand)
    {
        auto mul_res = detail::TryDecodeMulDiv(opcode, stream, ctx, out_unsupported);
        if (!mul_res)
            return mul_res.error();
        if (mul_res->has_value()) {
            return finalize(**mul_res);
        }
    }

    // 4. Bit / Ext single-byte (CDQ)
    {
        auto bit_res = detail::TryDecodeBitExt(opcode, stream, ctx, out_unsupported);
        if (!bit_res)
            return bit_res.error();
        if (bit_res->has_value()) {
            return finalize(**bit_res);
        }
    }

    // 5. Atomic / Exchange (XCHG single-byte & rm)
    {
        auto atomic_res = detail::TryDecodeAtomic(opcode, stream, ctx, out_unsupported);
        if (!atomic_res)
            return atomic_res.error();
        if (atomic_res->has_value()) {
            return finalize(**atomic_res);
        }
    }

    // 6. ALU & Group 1 & Group 3
    {
        auto alu_res = detail::TryDecodeAlu(opcode, stream, ctx, out_unsupported);
        if (!alu_res)
            return alu_res.error();
        if (alu_res->has_value()) {
            return finalize(**alu_res);
        }
    }

    // 7. Core data / movement / stack
    {
        auto data_res = detail::TryDecodeData(opcode, stream, ctx);
        if (!data_res)
            return data_res.error();
        if (data_res->has_value()) {
            return finalize(**data_res);
        }
    }

    DecodedInstruction inst;

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
        return finalize(inst);
    }

    // RET near (0xC3)
    if (opcode == opcodes::kRetNear) {
        inst.id = InstructionId::Ret;
        inst.ret_pop_bytes = 0;
        inst.cycles = cycles::kRet;
        inst.length = stream.length();
        return finalize(inst);
    }

    // RET imm16 (0xC2)
    if (opcode == opcodes::kRetNearImm16) {
        auto pop_res = stream.ReadImm16();
        if (!pop_res)
            return pop_res.error();
        inst.id = InstructionId::Ret;
        inst.ret_pop_bytes = *pop_res;
        inst.cycles = cycles::kRet;
        inst.length = stream.length();
        return finalize(inst);
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
        return finalize(inst);
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
        return finalize(inst);
    }

    // Group 5: INC /0, DEC /1, CALL /2, JMP /4, PUSH /6 (0xFF)
    if (opcode == opcodes::kGroup5) {
        auto modrm_res = ParseModRm(stream, ctx);
        if (!modrm_res)
            return modrm_res.error();
        inst.op1 = modrm_res->first;
        inst.length = stream.length();

        const u8 reg = modrm_res->second;
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
        return finalize(inst);
    }

    // INC r32 (0x40..0x47)
    if (opcode >= opcodes::kIncRegBase && opcode <= opcodes::kIncRegEnd) {
        inst.id = InstructionId::Inc;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kIncRegBase));
        inst.cycles = cycles::kAluRegReg;
        inst.length = stream.length();
        return finalize(inst);
    }

    // DEC r32 (0x48..0x4F)
    if (opcode >= opcodes::kDecRegBase && opcode <= opcodes::kDecRegEnd) {
        inst.id = InstructionId::Dec;
        inst.op1 = Operand::MakeReg(static_cast<Reg32>(opcode - opcodes::kDecRegBase));
        inst.cycles = cycles::kAluRegReg;
        inst.length = stream.length();
        return finalize(inst);
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
        return finalize(inst);
    }

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
        return finalize(inst);
    }

    // Two-byte escape (0x0F)
    if (opcode == opcodes::kTwoByteEscape) {
        auto sec_res = stream.NextByte();
        if (!sec_res)
            return sec_res.error();
        const u8 sec_opcode = *sec_res;

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
            return finalize(inst);
        }

        // Extended 2-byte instructions (MOVZX, MOVSX, BT, SETcc, IMUL, CMPXCHG)
        auto ext_res = detail::TryDecodeTwoByteExtended(sec_opcode, stream, ctx, out_unsupported);
        if (!ext_res)
            return ext_res.error();
        if (ext_res->has_value()) {
            return finalize(**ext_res);
        }

        return Error{ErrorCode::InvalidOpcode, "Prefixo 0x0F com opcode não suportado"};
    }

    // CLI (0xFA)
    if (opcode == opcodes::kCli) {
        inst.id = InstructionId::Cli;
        inst.cycles = cycles::kCli;
        inst.length = stream.length();
        return finalize(inst);
    }

    // STI (0xFB)
    if (opcode == opcodes::kSti) {
        inst.id = InstructionId::Sti;
        inst.cycles = cycles::kSti;
        inst.length = stream.length();
        return finalize(inst);
    }

    // IRET (0xCF)
    if (opcode == opcodes::kIret) {
        inst.id = InstructionId::Iret;
        inst.cycles = cycles::kIret;
        inst.length = stream.length();
        return finalize(inst);
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
        return finalize(inst);
    }

    return Error{ErrorCode::InvalidOpcode, "Opcode não suportado no subconjunto IA-32", opcode};
}

// Explicit template instantiations
template Result<DecodedInstruction>
Decoder::Decode<memory::AddressSpace>(const CpuContext& ctx, memory::AddressSpace& mem,
                                      GuestAddr eip,
                                      std::optional<UnsupportedFormInfo>* out_unsupported);

template Result<DecodedInstruction>
Decoder::Decode<memory::VirtualMemory>(const CpuContext& ctx, memory::VirtualMemory& mem,
                                       GuestAddr eip,
                                       std::optional<UnsupportedFormInfo>* out_unsupported);

} // namespace xblob::cpu
