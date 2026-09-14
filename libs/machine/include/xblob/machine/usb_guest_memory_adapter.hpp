#pragma once

#include "xblob/memory/address_space.hpp"
#include "xblob/usb/usb_memory.hpp"

namespace xblob::machine {

class AddressSpaceUsbGuestMemory final : public usb::UsbGuestMemory {
public:
    explicit AddressSpaceUsbGuestMemory(memory::AddressSpace& space) : space_(space) {}

    [[nodiscard]] Result<void> ReadGuest(GuestAddr addr, MutableByteSpan dest) noexcept override {
        return space_.ReadBytes(addr, dest);
    }

    [[nodiscard]] Result<void> WriteGuest(GuestAddr addr, ByteSpan src) noexcept override {
        return space_.WriteBytes(addr, src);
    }

    [[nodiscard]] bool IsValidRange(GuestAddr addr, GuestSize size) const noexcept override {
        return space_.ValidateRange(addr, size, memory::MemoryPermission::ReadWrite).has_value();
    }

private:
    memory::AddressSpace& space_;
};

} // namespace xblob::machine
