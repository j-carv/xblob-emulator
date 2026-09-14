#pragma once

#include "xblob/common/types.hpp"
#include "xblob/cpu/operand.hpp"
#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

enum class InstructionId : u16 {
    Unknown = 0,
    Nop,
    Hlt,
    Mov,
    Lea,
    Push,
    Pop,
    Pushf,
    Popf,
    Call,
    Ret,
    Add,
    Adc,
    Sub,
    Sbb,
    Cmp,
    Inc,
    Dec,
    And,
    Or,
    Xor,
    Test,
    Jmp,
    Jcc,
    Cli,
    Sti,
    Iret,
    Int,

    // Shifts and rotates
    Shl,
    Shr,
    Sar,
    Rol,
    Ror,

    // Multiply and divide
    Mul,
    Imul,
    Div,
    Idiv,

    // Bit manipulation and extensions
    Movzx,
    Movsx,
    Cdq,
    Bt,
    Setcc,

    // String operations
    Movs,
    Stos,
    Lods,
    Cmps,
    Scas,
    Cld,
    Std,

    // Atomic / exchange
    Xchg,
    Cmpxchg,
};

struct UnsupportedFormInfo {
    GuestAddr fault_eip{0};
    std::vector<u8> opcode_bytes{};
    std::string reason{};
};

enum class ConditionCode : u8 {
    O = 0x0,
    NO = 0x1,
    B = 0x2,   // C, NAE
    NB = 0x3,  // NC, AE
    Z = 0x4,   // E
    NZ = 0x5,  // NE
    BE = 0x6,  // NA
    NBE = 0x7, // A
    S = 0x8,
    NS = 0x9,
    P = 0xA,   // PE
    NP = 0xB,  // PO
    L = 0xC,   // NGE
    NL = 0xD,  // GE
    LE = 0xE,  // NG
    NLE = 0xF, // G
};

[[nodiscard]] constexpr bool EvaluateCondition(ConditionCode cond, u32 eflags) noexcept {
    const bool cf = (eflags & kFlagCF) != 0;
    const bool zf = (eflags & kFlagZF) != 0;
    const bool sf = (eflags & kFlagSF) != 0;
    const bool of = (eflags & kFlagOF) != 0;
    const bool pf = (eflags & kFlagPF) != 0;

    switch (cond) {
    case ConditionCode::O:
        return of;
    case ConditionCode::NO:
        return !of;
    case ConditionCode::B:
        return cf;
    case ConditionCode::NB:
        return !cf;
    case ConditionCode::Z:
        return zf;
    case ConditionCode::NZ:
        return !zf;
    case ConditionCode::BE:
        return cf || zf;
    case ConditionCode::NBE:
        return !cf && !zf;
    case ConditionCode::S:
        return sf;
    case ConditionCode::NS:
        return !sf;
    case ConditionCode::P:
        return pf;
    case ConditionCode::NP:
        return !pf;
    case ConditionCode::L:
        return sf != of;
    case ConditionCode::NL:
        return sf == of;
    case ConditionCode::LE:
        return zf || (sf != of);
    case ConditionCode::NLE:
        return !zf && (sf == of);
    }
    return false;
}

struct DecodedInstruction {
    InstructionId id{InstructionId::Unknown};
    u8 length{0};
    Cycle cycles{1};
    Operand op1{};
    Operand op2{};
    Operand op3{};
    ConditionCode condition{ConditionCode::Z};
    u32 branch_target{0};
    u16 ret_pop_bytes{0};
    u8 int_vector{0};
    bool rep{false};
    bool repne{false};
    bool lock{false};
    bool op_size_override{false};
    bool addr_size_override{false};
    u8 data_size{4};
};

} // namespace xblob::cpu
