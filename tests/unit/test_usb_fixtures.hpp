#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/usb/ohci_descriptors.hpp"
#include "xblob/usb/usb_memory.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace xblob::testing {

class SyntheticUsbMemory final : public usb::UsbGuestMemory {
public:
    explicit SyntheticUsbMemory(std::size_t size = 1024 * 1024) : buffer_(size, 0) {}

    [[nodiscard]] Result<void> ReadGuest(GuestAddr addr, MutableByteSpan dest) noexcept override {
        if (!IsValidRange(addr, static_cast<GuestSize>(dest.size()))) {
            return Error{ErrorCode::OutOfBounds, "Acesso fora dos limites de memória sintética",
                         addr};
        }
        std::memcpy(dest.data(), buffer_.data() + addr, dest.size());
        return Result<void>::Ok();
    }

    [[nodiscard]] Result<void> WriteGuest(GuestAddr addr, ByteSpan src) noexcept override {
        if (!IsValidRange(addr, static_cast<GuestSize>(src.size()))) {
            return Error{ErrorCode::OutOfBounds, "Escrita fora dos limites de memória sintética",
                         addr};
        }
        std::memcpy(buffer_.data() + addr, src.data(), src.size());
        return Result<void>::Ok();
    }

    [[nodiscard]] bool IsValidRange(GuestAddr addr, GuestSize size) const noexcept override {
        if (fault_enabled_ && addr >= fault_addr_start_ && addr < fault_addr_end_) {
            return false;
        }
        u64 end = static_cast<u64>(addr) + static_cast<u64>(size);
        return end <= buffer_.size();
    }

    void SetFaultRange(GuestAddr start, GuestAddr end) noexcept {
        fault_enabled_ = true;
        fault_addr_start_ = start;
        fault_addr_end_ = end;
    }

    void ClearFault() noexcept { fault_enabled_ = false; }

    [[nodiscard]] GuestAddr AllocateAligned(u32 bytes, u32 alignment = 16) {
        current_alloc_ = (current_alloc_ + alignment - 1) & ~(alignment - 1);
        GuestAddr allocated = current_alloc_;
        current_alloc_ += bytes;
        return allocated;
    }

    [[nodiscard]] GuestAddr WriteEndpointDescriptor(const usb::ohci::EndpointDescriptor& ed) {
        GuestAddr addr = AllocateAligned(16, 16);
        (void)Write32(addr, ed.flags);
        (void)Write32(addr + 4, ed.tail_p);
        (void)Write32(addr + 8, ed.head_p);
        (void)Write32(addr + 12, ed.next_ed);
        return addr;
    }

    [[nodiscard]] GuestAddr
    WriteTransferDescriptor(const usb::ohci::GeneralTransferDescriptor& td) {
        GuestAddr addr = AllocateAligned(16, 16);
        (void)Write32(addr, td.flags);
        (void)Write32(addr + 4, td.cbp);
        (void)Write32(addr + 8, td.next_td);
        (void)Write32(addr + 12, td.be);
        return addr;
    }

    [[nodiscard]] std::vector<u8>& raw_buffer() noexcept { return buffer_; }

private:
    std::vector<u8> buffer_;
    GuestAddr current_alloc_{0x1000}; // Start allocations at 4 KiB
    bool fault_enabled_{false};
    GuestAddr fault_addr_start_{0};
    GuestAddr fault_addr_end_{0};
};

} // namespace xblob::testing
