#pragma once

#include "xblob/common/types.hpp"

#include <array>

namespace xblob::cpu {

enum class Reg32 : u8 { EAX = 0, ECX = 1, EDX = 2, EBX = 3, ESP = 4, EBP = 5, ESI = 6, EDI = 7 };

enum class SegmentReg : u8 { CS = 0, DS = 1, SS = 2, ES = 3, FS = 4, GS = 5 };

inline constexpr u32 kFlagCF = 1u << 0;
inline constexpr u32 kFlagReserved1 = 1u << 1; // Bit 1 sempre 1 no IA-32
inline constexpr u32 kFlagPF = 1u << 2;
inline constexpr u32 kFlagAF = 1u << 4;
inline constexpr u32 kFlagZF = 1u << 6;
inline constexpr u32 kFlagSF = 1u << 7;
inline constexpr u32 kFlagTF = 1u << 8;
inline constexpr u32 kFlagIF = 1u << 9;
inline constexpr u32 kFlagDF = 1u << 10;
inline constexpr u32 kFlagOF = 1u << 11;

struct Idtr {
    u32 base{0};
    u16 limit{0};
};

struct CpuContext {
    std::array<u32, 8> gpr{0, 0, 0, 0, 0, 0, 0, 0};
    u32 eip{0};
    u32 eflags{kFlagReserved1};
    std::array<u16, 6> segments{0, 0, 0, 0, 0, 0};
    u32 cr0{0};
    u32 cr2{0};
    u32 cr3{0};
    Idtr idtr{0, 0};

    void Reset() noexcept;

    [[nodiscard]] u32 GetGpr(Reg32 reg) const noexcept {
        return gpr[static_cast<std::size_t>(reg)];
    }

    void SetGpr(Reg32 reg, u32 val) noexcept { gpr[static_cast<std::size_t>(reg)] = val; }

    [[nodiscard]] u16 GetSegment(SegmentReg seg) const noexcept {
        return segments[static_cast<std::size_t>(seg)];
    }

    void SetSegment(SegmentReg seg, u16 val) noexcept {
        segments[static_cast<std::size_t>(seg)] = val;
    }

    [[nodiscard]] bool GetFlag(u32 flag_mask) const noexcept { return (eflags & flag_mask) != 0; }

    void SetFlag(u32 flag_mask, bool val) noexcept {
        if (val) {
            eflags |= flag_mask;
        } else {
            eflags &= ~flag_mask;
        }
        eflags |= kFlagReserved1; // Invariante bit 1
    }

    void SetEflags(u32 val) noexcept {
        eflags = val | kFlagReserved1; // Invariante bit 1
    }
};

} // namespace xblob::cpu
