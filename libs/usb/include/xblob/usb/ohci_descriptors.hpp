#pragma once

#include "xblob/common/types.hpp"
#include "xblob/usb/usb_types.hpp"

namespace xblob::usb::ohci {

// Alignment requirement for OHCI descriptors in guest memory
inline constexpr u32 kDescriptorAlignment = 16;
inline constexpr u32 kMaxEdTraversalBudget = 64;
inline constexpr u32 kMaxTdTraversalBudget = 128;

struct EndpointDescriptor {
    u32 flags{0};
    u32 tail_p{0};
    u32 head_p{0};
    u32 next_ed{0};

    [[nodiscard]] u8 function_address() const noexcept { return static_cast<u8>(flags & 0x7F); }

    [[nodiscard]] u8 endpoint_number() const noexcept {
        return static_cast<u8>((flags >> 7) & 0x0F);
    }

    [[nodiscard]] u8 direction() const noexcept {
        // 00b = From TD, 01b = OUT, 10b = IN, 11b = From TD
        return static_cast<u8>((flags >> 11) & 0x03);
    }

    [[nodiscard]] UsbSpeed speed() const noexcept {
        return ((flags >> 13) & 0x01) ? UsbSpeed::LowSpeed : UsbSpeed::FullSpeed;
    }

    [[nodiscard]] bool skip() const noexcept { return ((flags >> 14) & 0x01) != 0; }

    [[nodiscard]] bool is_isochronous() const noexcept { return ((flags >> 15) & 0x01) != 0; }

    [[nodiscard]] u16 max_packet_size() const noexcept {
        return static_cast<u16>((flags >> 16) & 0x07FF);
    }

    [[nodiscard]] bool is_halted() const noexcept { return (head_p & 0x01) != 0; }

    void set_halted(bool halted) noexcept {
        if (halted) {
            head_p |= 0x01;
        } else {
            head_p &= ~0x01U;
        }
    }

    [[nodiscard]] bool toggle_carry() const noexcept { return (head_p & 0x02) != 0; }

    void set_toggle_carry(bool carry) noexcept {
        if (carry) {
            head_p |= 0x02;
        } else {
            head_p &= ~0x02U;
        }
    }

    [[nodiscard]] u32 head_pointer() const noexcept { return head_p & ~0x0FU; }

    void set_head_pointer(u32 pointer) noexcept { head_p = (pointer & ~0x0FU) | (head_p & 0x0FU); }

    [[nodiscard]] u32 tail_pointer() const noexcept { return tail_p & ~0x0FU; }

    [[nodiscard]] u32 next_ed_pointer() const noexcept { return next_ed & ~0x0FU; }

    [[nodiscard]] bool is_queue_empty() const noexcept { return head_pointer() == tail_pointer(); }
};

struct GeneralTransferDescriptor {
    u32 flags{0};
    u32 cbp{0};
    u32 next_td{0};
    u32 be{0};

    [[nodiscard]] bool buffer_rounding() const noexcept { return ((flags >> 18) & 0x01) != 0; }

    [[nodiscard]] UsbPid pid_direction() const noexcept {
        u8 dp = static_cast<u8>((flags >> 19) & 0x03);
        switch (dp) {
        case 0:
            return UsbPid::Setup;
        case 1:
            return UsbPid::Out;
        case 2:
            return UsbPid::In;
        default:
            return UsbPid::Reserved;
        }
    }

    [[nodiscard]] u8 delay_interrupt() const noexcept {
        return static_cast<u8>((flags >> 21) & 0x07);
    }

    [[nodiscard]] u8 data_toggle() const noexcept { return static_cast<u8>((flags >> 24) & 0x03); }

    [[nodiscard]] u8 error_count() const noexcept { return static_cast<u8>((flags >> 26) & 0x03); }

    void set_error_count(u8 count) noexcept {
        flags = (flags & ~(0x03U << 26)) | (static_cast<u32>(count & 0x03) << 26);
    }

    [[nodiscard]] u8 condition_code() const noexcept {
        return static_cast<u8>((flags >> 28) & 0x0F);
    }

    void set_condition_code(u8 cc) noexcept {
        flags = (flags & ~(0x0FU << 28)) | (static_cast<u32>(cc & 0x0F) << 28);
    }

    [[nodiscard]] u32 buffer_length() const noexcept {
        if (cbp == 0) {
            return 0;
        }
        if (be < cbp) {
            // Can cross page boundary in OHCI (2 pages max)
            u32 cbp_page = cbp & ~0xFFFU;
            u32 be_page = be & ~0xFFFU;
            if (be_page > cbp_page) {
                return (0x1000 - (cbp & 0xFFF)) + (be & 0xFFF) + 1;
            }
            return 0; // Malformed
        }
        return (be - cbp) + 1;
    }

    [[nodiscard]] u32 next_td_pointer() const noexcept { return next_td & ~0x0FU; }
};

} // namespace xblob::usb::ohci
