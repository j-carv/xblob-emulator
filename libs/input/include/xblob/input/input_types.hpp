#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace xblob::input {

namespace buttons {
inline constexpr u8 kDPadUp = 1 << 0;
inline constexpr u8 kDPadDown = 1 << 1;
inline constexpr u8 kDPadLeft = 1 << 2;
inline constexpr u8 kDPadRight = 1 << 3;
inline constexpr u8 kStart = 1 << 4;
inline constexpr u8 kBack = 1 << 5;
inline constexpr u8 kLeftThumb = 1 << 6;
inline constexpr u8 kRightThumb = 1 << 7;
} // namespace buttons

struct HostInputSnapshot {
    u64 sequence{0};
    bool connected{true};

    // Digital buttons (D-pad, Start, Back, Thumbs)
    u8 digital_buttons{0};

    // Analog pressure-sensitive buttons (0 to 255)
    u8 button_a{0};
    u8 button_b{0};
    u8 button_x{0};
    u8 button_y{0};
    u8 button_black{0};
    u8 button_white{0};

    // Triggers (0 to 255)
    u8 trigger_left{0};
    u8 trigger_right{0};

    // Thumbsticks (-32768 to 32767)
    i16 thumb_lx{0};
    i16 thumb_ly{0};
    i16 thumb_rx{0};
    i16 thumb_ry{0};

    void ClampAndNormalize() noexcept {
        // Clamping ensures values stay within valid physical bounds
        thumb_lx = std::clamp<i16>(thumb_lx, -32768, 32767);
        thumb_ly = std::clamp<i16>(thumb_ly, -32768, 32767);
        thumb_rx = std::clamp<i16>(thumb_rx, -32768, 32767);
        thumb_ry = std::clamp<i16>(thumb_ry, -32768, 32767);
    }
};

inline constexpr u32 kXidGamepadReportSize = 20;

struct XidGamepadReport {
    u8 report_id{0x00};
    u8 report_size{static_cast<u8>(kXidGamepadReportSize)};
    u8 buttons{0x00};
    u8 reserved{0x00};
    u8 a{0};
    u8 b{0};
    u8 x{0};
    u8 y{0};
    u8 black{0};
    u8 white{0};
    u8 left_trigger{0};
    u8 right_trigger{0};
    i16 thumb_lx{0};
    i16 thumb_ly{0};
    i16 thumb_rx{0};
    i16 thumb_ry{0};

    static XidGamepadReport FromSnapshot(const HostInputSnapshot& snapshot) noexcept {
        XidGamepadReport report;
        report.report_id = 0x00;
        report.report_size = static_cast<u8>(kXidGamepadReportSize);
        report.buttons = snapshot.digital_buttons;
        report.reserved = 0x00;
        report.a = snapshot.button_a;
        report.b = snapshot.button_b;
        report.x = snapshot.button_x;
        report.y = snapshot.button_y;
        report.black = snapshot.button_black;
        report.white = snapshot.button_white;
        report.left_trigger = snapshot.trigger_left;
        report.right_trigger = snapshot.trigger_right;
        report.thumb_lx = snapshot.thumb_lx;
        report.thumb_ly = snapshot.thumb_ly;
        report.thumb_rx = snapshot.thumb_rx;
        report.thumb_ry = snapshot.thumb_ry;
        return report;
    }

    [[nodiscard]] Result<void> Serialize(MutableByteSpan dest) const noexcept {
        if (dest.size() < kXidGamepadReportSize) {
            return Error{ErrorCode::OutOfBounds, "Buffer de destino pequeno demais para report XID",
                         dest.size()};
        }
        dest[0] = report_id;
        dest[1] = report_size;
        dest[2] = buttons;
        dest[3] = reserved;
        dest[4] = a;
        dest[5] = b;
        dest[6] = x;
        dest[7] = y;
        dest[8] = black;
        dest[9] = white;
        dest[10] = left_trigger;
        dest[11] = right_trigger;

        // Little-endian serialization of signed 16-bit stick axes
        auto u16_lx = static_cast<u16>(thumb_lx);
        dest[12] = static_cast<u8>(u16_lx & 0xFF);
        dest[13] = static_cast<u8>((u16_lx >> 8) & 0xFF);

        auto u16_ly = static_cast<u16>(thumb_ly);
        dest[14] = static_cast<u8>(u16_ly & 0xFF);
        dest[15] = static_cast<u8>((u16_ly >> 8) & 0xFF);

        auto u16_rx = static_cast<u16>(thumb_rx);
        dest[16] = static_cast<u8>(u16_rx & 0xFF);
        dest[17] = static_cast<u8>((u16_rx >> 8) & 0xFF);

        auto u16_ry = static_cast<u16>(thumb_ry);
        dest[18] = static_cast<u8>(u16_ry & 0xFF);
        dest[19] = static_cast<u8>((u16_ry >> 8) & 0xFF);

        return Result<void>::Ok();
    }

    [[nodiscard]] static Result<XidGamepadReport> Deserialize(ByteSpan src) noexcept {
        if (src.size() < kXidGamepadReportSize) {
            return Error{ErrorCode::TruncatedData, "Buffer menor que o tamanho do report XID",
                         src.size()};
        }
        XidGamepadReport r;
        r.report_id = src[0];
        r.report_size = src[1];
        r.buttons = src[2];
        r.reserved = src[3];
        r.a = src[4];
        r.b = src[5];
        r.x = src[6];
        r.y = src[7];
        r.black = src[8];
        r.white = src[9];
        r.left_trigger = src[10];
        r.right_trigger = src[11];

        u16 lx = static_cast<u16>(static_cast<u16>(src[12]) |
                                  static_cast<u16>(static_cast<u16>(src[13]) << 8));
        u16 ly = static_cast<u16>(static_cast<u16>(src[14]) |
                                  static_cast<u16>(static_cast<u16>(src[15]) << 8));
        u16 rx = static_cast<u16>(static_cast<u16>(src[16]) |
                                  static_cast<u16>(static_cast<u16>(src[17]) << 8));
        u16 ry = static_cast<u16>(static_cast<u16>(src[18]) |
                                  static_cast<u16>(static_cast<u16>(src[19]) << 8));

        r.thumb_lx = static_cast<i16>(lx);
        r.thumb_ly = static_cast<i16>(ly);
        r.thumb_rx = static_cast<i16>(rx);
        r.thumb_ry = static_cast<i16>(ry);

        return r;
    }
};

} // namespace xblob::input
