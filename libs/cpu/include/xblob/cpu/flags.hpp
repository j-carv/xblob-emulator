#pragma once

#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

inline constexpr u32 kArithmeticFlagsMask =
    kFlagCF | kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF;

[[nodiscard]] constexpr bool CalculateParity(u8 byte) noexcept {
    u8 count = 0;
    for (int i = 0; i < 8; ++i) {
        if (((byte >> i) & 1) != 0) {
            count++;
        }
    }
    return (count % 2) == 0;
}

[[nodiscard]] constexpr u32 CalculateAddFlags(u32 a, u32 b, u32 current_flags) noexcept {
    const u64 sum = static_cast<u64>(a) + static_cast<u64>(b);
    const u32 res = static_cast<u32>(sum);

    const bool cf = (sum > 0xFFFFFFFFULL);
    const bool af = ((a & 0x0F) + (b & 0x0F)) > 0x0F;
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = ((a ^ res) & (b ^ res) & 0x80000000U) != 0;
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateAdcFlags(u32 a, u32 b, bool cf_in,
                                              u32 current_flags) noexcept {
    const u64 cin = cf_in ? 1ULL : 0ULL;
    const u64 sum = static_cast<u64>(a) + static_cast<u64>(b) + cin;
    const u32 res = static_cast<u32>(sum);

    const bool cf = (sum > 0xFFFFFFFFULL);
    const bool af = ((a & 0x0F) + (b & 0x0F) + static_cast<u32>(cin)) > 0x0F;
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = ((~(a ^ b)) & (a ^ res) & 0x80000000U) != 0;
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateSubFlags(u32 a, u32 b, u32 current_flags) noexcept {
    const u32 res = a - b;

    const bool cf = (a < b);
    const bool af = (a & 0x0F) < (b & 0x0F);
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = ((a ^ b) & (a ^ res) & 0x80000000U) != 0;
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateSubFlags8(u8 a, u8 b, u32 current_flags) noexcept {
    const u8 res = static_cast<u8>(a - b);

    const bool cf = (a < b);
    const bool af = (a & 0x0F) < (b & 0x0F);
    const bool zf = (res == 0);
    const bool sf = (res & 0x80) != 0;
    const bool of = (((a ^ b) & (a ^ res) & 0x80) != 0);
    const bool pf = CalculateParity(res);

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateSubFlags16(u16 a, u16 b, u32 current_flags) noexcept {
    const u16 res = static_cast<u16>(a - b);

    const bool cf = (a < b);
    const bool af = (a & 0x0F) < (b & 0x0F);
    const bool zf = (res == 0);
    const bool sf = (res & 0x8000) != 0;
    const bool of = (((a ^ b) & (a ^ res) & 0x8000) != 0);
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateSbbFlags(u32 a, u32 b, bool cf_in,
                                              u32 current_flags) noexcept {
    const u64 cin = cf_in ? 1ULL : 0ULL;
    const u32 res = a - b - static_cast<u32>(cin);

    const bool cf = static_cast<u64>(a) < (static_cast<u64>(b) + cin);
    const bool af = (a & 0x0F) < ((b & 0x0F) + static_cast<u32>(cin));
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = ((a ^ b) & (a ^ res) & 0x80000000U) != 0;
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (cf)
        flags |= kFlagCF;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateIncFlags(u32 a, u32 current_flags) noexcept {
    const u32 res = a + 1;

    const bool af = (a & 0x0F) == 0x0F;
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = (a == 0x7FFFFFFFU);
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    // INC preserves CF!
    u32 flags =
        (current_flags & ~(kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF)) | kFlagReserved1;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateDecFlags(u32 a, u32 current_flags) noexcept {
    const u32 res = a - 1;

    const bool af = (a & 0x0F) == 0x00;
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool of = (a == 0x80000000U);
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    // DEC preserves CF!
    u32 flags =
        (current_flags & ~(kFlagPF | kFlagAF | kFlagZF | kFlagSF | kFlagOF)) | kFlagReserved1;
    if (pf)
        flags |= kFlagPF;
    if (af)
        flags |= kFlagAF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;
    if (of)
        flags |= kFlagOF;

    return flags;
}

[[nodiscard]] constexpr u32 CalculateLogicFlags(u32 res, u32 current_flags) noexcept {
    const bool zf = (res == 0);
    const bool sf = (res & 0x80000000U) != 0;
    const bool pf = CalculateParity(static_cast<u8>(res & 0xFF));

    // Logic instructions clear CF and OF, AF undefined (cleared)
    u32 flags = (current_flags & ~kArithmeticFlagsMask) | kFlagReserved1;
    if (pf)
        flags |= kFlagPF;
    if (zf)
        flags |= kFlagZF;
    if (sf)
        flags |= kFlagSF;

    return flags;
}

} // namespace xblob::cpu
