#pragma once

#include "xblob/common/types.hpp"

namespace xblob::cpu {

namespace opcodes {
inline constexpr u8 kNop = 0x90;
inline constexpr u8 kHlt = 0xF4;

// MOV
inline constexpr u8 kMovRegImmBase = 0xB8; // 0xB8 + rd (0..7)
inline constexpr u8 kMovRegImmEnd = 0xBF;
inline constexpr u8 kMovRmReg32 = 0x89;
inline constexpr u8 kMovRegRm32 = 0x8B;
inline constexpr u8 kMovRmImm32 = 0xC7; // reg = 0
inline constexpr u8 kMovEaxMoffs32 = 0xA1;
inline constexpr u8 kMovMoffs32Eax = 0xA3;

// LEA
inline constexpr u8 kLea = 0x8D;

// Stack
inline constexpr u8 kPushRegBase = 0x50; // 0x50 + rd (0..7)
inline constexpr u8 kPushRegEnd = 0x57;
inline constexpr u8 kPushImm32 = 0x68;
inline constexpr u8 kPushImm8 = 0x6A;
inline constexpr u8 kPopRegBase = 0x58; // 0x58 + rd (0..7)
inline constexpr u8 kPopRegEnd = 0x5F;
inline constexpr u8 kPopRm32 = 0x8F; // reg = 0
inline constexpr u8 kPushf = 0x9C;
inline constexpr u8 kPopf = 0x9D;

// Flow
inline constexpr u8 kCallRel32 = 0xE8;
inline constexpr u8 kRetNear = 0xC3;
inline constexpr u8 kRetNearImm16 = 0xC2;
inline constexpr u8 kJmpRel8 = 0xEB;
inline constexpr u8 kJmpRel32 = 0xE9;

// Group 5 (0xFF): INC /0, DEC /1, CALL /2, JMP /4, PUSH /6
inline constexpr u8 kGroup5 = 0xFF;

// ALU 32-bit
inline constexpr u8 kAddRmReg32 = 0x01;
inline constexpr u8 kAddRegRm32 = 0x03;
inline constexpr u8 kAddEaxImm32 = 0x05;

inline constexpr u8 kAdcRmReg32 = 0x11;
inline constexpr u8 kAdcRegRm32 = 0x13;
inline constexpr u8 kAdcEaxImm32 = 0x15;

inline constexpr u8 kSubRmReg32 = 0x29;
inline constexpr u8 kSubRegRm32 = 0x2B;
inline constexpr u8 kSubEaxImm32 = 0x2D;

inline constexpr u8 kSbbRmReg32 = 0x19;
inline constexpr u8 kSbbRegRm32 = 0x1B;
inline constexpr u8 kSbbEaxImm32 = 0x1D;

inline constexpr u8 kCmpRmReg32 = 0x39;
inline constexpr u8 kCmpRegRm32 = 0x3B;
inline constexpr u8 kCmpEaxImm32 = 0x3D;

inline constexpr u8 kIncRegBase = 0x40; // 0x40 + rd
inline constexpr u8 kIncRegEnd = 0x47;
inline constexpr u8 kDecRegBase = 0x48; // 0x48 + rd
inline constexpr u8 kDecRegEnd = 0x4F;

// Logic 32-bit
inline constexpr u8 kAndRmReg32 = 0x21;
inline constexpr u8 kAndRegRm32 = 0x23;
inline constexpr u8 kAndEaxImm32 = 0x25;

inline constexpr u8 kOrRmReg32 = 0x09;
inline constexpr u8 kOrRegRm32 = 0x0B;
inline constexpr u8 kOrEaxImm32 = 0x0D;

inline constexpr u8 kXorRmReg32 = 0x31;
inline constexpr u8 kXorRegRm32 = 0x33;
inline constexpr u8 kXorEaxImm32 = 0x35;

inline constexpr u8 kTestRmReg32 = 0x85;
inline constexpr u8 kTestEaxImm32 = 0xA9;
inline constexpr u8 kTestRmImm32Group3 = 0xF7; // reg = 0

// Group 1 (0x81: imm32, 0x83: imm8 sign-extended)
// /0 ADD, /1 OR, /2 ADC, /3 SBB, /4 AND, /5 SUB, /6 XOR, /7 CMP
inline constexpr u8 kGroup1Imm32 = 0x81;
inline constexpr u8 kGroup1Imm8 = 0x83;

// Jcc rel8: 0x70 .. 0x7F
inline constexpr u8 kJccRel8Base = 0x70;
inline constexpr u8 kJccRel8End = 0x7F;

// Two-byte prefix (0x0F)
inline constexpr u8 kTwoByteEscape = 0x0F;
inline constexpr u8 kJccRel32Base = 0x80; // After 0x0F: 0x80 .. 0x8F
inline constexpr u8 kJccRel32End = 0x8F;

// Control / Interrupt
inline constexpr u8 kCli = 0xFA;
inline constexpr u8 kSti = 0xFB;
inline constexpr u8 kIret = 0xCF;
inline constexpr u8 kIntImm8 = 0xCD;

} // namespace opcodes

namespace cycles {
inline constexpr Cycle kNop = 1;
inline constexpr Cycle kHlt = 1;
inline constexpr Cycle kMovRegImm = 1;
inline constexpr Cycle kMovRegReg = 1;
inline constexpr Cycle kMovMem = 2;
inline constexpr Cycle kMovEaxMoffs32 = 2;
inline constexpr Cycle kMovMoffs32Eax = 2;
inline constexpr Cycle kLea = 1;

inline constexpr Cycle kPushReg = 1;
inline constexpr Cycle kPushImm = 1;
inline constexpr Cycle kPushMem = 2;
inline constexpr Cycle kPopReg = 1;
inline constexpr Cycle kPopMem = 2;
inline constexpr Cycle kPushf = 2;
inline constexpr Cycle kPopf = 2;

inline constexpr Cycle kCallRel32 = 2;
inline constexpr Cycle kCallRm = 3;
inline constexpr Cycle kRet = 2;

inline constexpr Cycle kAluRegReg = 1;
inline constexpr Cycle kAluRegImm = 1;
inline constexpr Cycle kAluMem = 2;

inline constexpr Cycle kJmpRel8 = 1;
inline constexpr Cycle kJmpRel32 = 1;
inline constexpr Cycle kJmpRm = 2;
inline constexpr Cycle kJccTaken = 1;
inline constexpr Cycle kJccNotTaken = 1;

inline constexpr Cycle kCli = 1;
inline constexpr Cycle kSti = 1;
inline constexpr Cycle kIret = 4;
inline constexpr Cycle kInt = 4;
} // namespace cycles

} // namespace xblob::cpu
