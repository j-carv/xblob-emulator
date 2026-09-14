#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

namespace xblob::usb {

class UsbGuestMemory {
public:
    virtual ~UsbGuestMemory() = default;

    [[nodiscard]] virtual Result<void> ReadGuest(GuestAddr addr, MutableByteSpan dest) noexcept = 0;
    [[nodiscard]] virtual Result<void> WriteGuest(GuestAddr addr, ByteSpan src) noexcept = 0;
    [[nodiscard]] virtual bool IsValidRange(GuestAddr addr, GuestSize size) const noexcept = 0;

    [[nodiscard]] Result<u32> Read32(GuestAddr addr) noexcept {
        u8 buf[4]{0};
        auto res = ReadGuest(addr, MutableByteSpan{buf, 4});
        if (!res) {
            return res.error();
        }
        u32 val = static_cast<u32>(buf[0]) | (static_cast<u32>(buf[1]) << 8) |
                  (static_cast<u32>(buf[2]) << 16) | (static_cast<u32>(buf[3]) << 24);
        return val;
    }

    [[nodiscard]] Result<void> Write32(GuestAddr addr, u32 val) noexcept {
        u8 buf[4]{
            static_cast<u8>(val & 0xFF),
            static_cast<u8>((val >> 8) & 0xFF),
            static_cast<u8>((val >> 16) & 0xFF),
            static_cast<u8>((val >> 24) & 0xFF),
        };
        return WriteGuest(addr, ByteSpan{buf, 4});
    }
};

} // namespace xblob::usb
