#include "xblob/pci/pci_config_header.hpp"

#include "xblob/common/safe_math.hpp"

#include <cstring>

namespace xblob::pci {

PciConfigHeader::PciConfigHeader() {
    raw_bytes_.fill(0);
    // Header Type 0 (standard single-function device)
    raw_bytes_[kPciHeaderTypeOffset] = 0x00;
    // Status defaults: Devsel medium, 66MHz capable
    const u16 default_status = kPciStatusDevselMedium | kPciStatus66MHz;
    raw_bytes_[kPciStatusOffset] = static_cast<u8>(default_status & 0xFF);
    raw_bytes_[kPciStatusOffset + 1] = static_cast<u8>((default_status >> 8) & 0xFF);
}

void PciConfigHeader::SetVendorId(u16 vendor_id) noexcept {
    raw_bytes_[kPciVendorIdOffset] = static_cast<u8>(vendor_id & 0xFF);
    raw_bytes_[kPciVendorIdOffset + 1] = static_cast<u8>((vendor_id >> 8) & 0xFF);
}

void PciConfigHeader::SetDeviceId(u16 device_id) noexcept {
    raw_bytes_[kPciDeviceIdOffset] = static_cast<u8>(device_id & 0xFF);
    raw_bytes_[kPciDeviceIdOffset + 1] = static_cast<u8>((device_id >> 8) & 0xFF);
}

void PciConfigHeader::SetClassCodes(u8 base_class, u8 subclass, u8 prog_if) noexcept {
    raw_bytes_[kPciProgIfOffset] = prog_if;
    raw_bytes_[kPciSubclassOffset] = subclass;
    raw_bytes_[kPciBaseClassOffset] = base_class;
}

void PciConfigHeader::SetRevisionId(u8 revision_id) noexcept {
    raw_bytes_[kPciRevisionIdOffset] = revision_id;
}

void PciConfigHeader::SetSubsystemIds(u16 sub_vendor_id, u16 sub_device_id) noexcept {
    raw_bytes_[kPciSubsystemVendorIdOffset] = static_cast<u8>(sub_vendor_id & 0xFF);
    raw_bytes_[kPciSubsystemVendorIdOffset + 1] = static_cast<u8>((sub_vendor_id >> 8) & 0xFF);
    raw_bytes_[kPciSubsystemIdOffset] = static_cast<u8>(sub_device_id & 0xFF);
    raw_bytes_[kPciSubsystemIdOffset + 1] = static_cast<u8>((sub_device_id >> 8) & 0xFF);
}

void PciConfigHeader::SetInterrupt(u8 line, u8 pin) noexcept {
    raw_bytes_[kPciInterruptLineOffset] = line;
    raw_bytes_[kPciInterruptPinOffset] = pin;
}

Result<void> PciConfigHeader::ConfigureBar(u8 index, GuestSize size, bool is_io,
                                           bool is_prefetchable,
                                           std::shared_ptr<bus::BusDevice> device) {
    if (index >= kPciBarCount) {
        return Error{ErrorCode::OutOfBounds, "PCI BAR index out of bounds"};
    }
    bars_[index] = PciBar(index, size, is_io, is_prefetchable, std::move(device));
    NotifyChange();
    return {};
}

PciBar& PciConfigHeader::bar(u8 index) {
    if (index >= kPciBarCount) {
        static PciBar s_dummy;
        return s_dummy;
    }
    return bars_[index];
}

const PciBar& PciConfigHeader::bar(u8 index) const {
    if (index >= kPciBarCount) {
        static const PciBar s_dummy;
        return s_dummy;
    }
    return bars_[index];
}

u16 PciConfigHeader::vendor_id() const noexcept {
    return static_cast<u16>(raw_bytes_[kPciVendorIdOffset] |
                            (static_cast<u16>(raw_bytes_[kPciVendorIdOffset + 1]) << 8));
}

u16 PciConfigHeader::device_id() const noexcept {
    return static_cast<u16>(raw_bytes_[kPciDeviceIdOffset] |
                            (static_cast<u16>(raw_bytes_[kPciDeviceIdOffset + 1]) << 8));
}

u16 PciConfigHeader::command() const noexcept {
    return static_cast<u16>(raw_bytes_[kPciCommandOffset] |
                            (static_cast<u16>(raw_bytes_[kPciCommandOffset + 1]) << 8));
}

u16 PciConfigHeader::status() const noexcept {
    return static_cast<u16>(raw_bytes_[kPciStatusOffset] |
                            (static_cast<u16>(raw_bytes_[kPciStatusOffset + 1]) << 8));
}

bool PciConfigHeader::is_memory_space_enabled() const noexcept {
    return (command() & kPciCommandMemorySpace) != 0;
}

bool PciConfigHeader::is_io_space_enabled() const noexcept {
    return (command() & kPciCommandIoSpace) != 0;
}

void PciConfigHeader::NotifyChange() noexcept {
    if (change_callback_) {
        change_callback_();
    }
}

u32 PciConfigHeader::ReadRaw(u8 offset, PciAccessWidth width) const noexcept {
    const auto w = static_cast<u8>(width);
    u32 value = 0;
    for (u8 i = 0; i < w; ++i) {
        value |= (static_cast<u32>(raw_bytes_[offset + i]) << (i * 8));
    }
    return value;
}

void PciConfigHeader::WriteRaw(u8 offset, PciAccessWidth width, u32 value) noexcept {
    const auto w = static_cast<u8>(width);
    for (u8 i = 0; i < w; ++i) {
        raw_bytes_[offset + i] = static_cast<u8>((value >> (i * 8)) & 0xFF);
    }
}

Result<u32> PciConfigHeader::Read(u8 offset, PciAccessWidth width) const noexcept {
    const auto w = static_cast<std::size_t>(width);
    if (!RangeInBounds<std::size_t>(offset, w, kPciConfigSpaceSize)) {
        return Error{ErrorCode::OutOfBounds, "PCI configuration read out of bounds", offset};
    }

    // Alignment checking
    if (width == PciAccessWidth::Word && (offset % 2 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 16-bit PCI configuration read",
                     offset};
    }
    if (width == PciAccessWidth::Dword && (offset % 4 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit PCI configuration read",
                     offset};
    }

    // Intercept BAR accesses
    if (offset >= kPciBar0Offset && offset < kPciBar5Offset + 4) {
        const u8 bar_idx = (offset - kPciBar0Offset) / 4;
        const u8 bar_byte_offset = (offset - kPciBar0Offset) % 4;
        const u32 bar_val = bars_[bar_idx].Read();
        if (width == PciAccessWidth::Dword && bar_byte_offset == 0) {
            return bar_val;
        }
        if (width == PciAccessWidth::Word) {
            return (bar_val >> (bar_byte_offset * 8)) & 0xFFFFu;
        }
        return (bar_val >> (bar_byte_offset * 8)) & 0xFFu;
    }

    return ReadRaw(offset, width);
}

Result<void> PciConfigHeader::Write(u8 offset, PciAccessWidth width, u32 value) {
    const auto w = static_cast<std::size_t>(width);
    if (!RangeInBounds<std::size_t>(offset, w, kPciConfigSpaceSize)) {
        return Error{ErrorCode::OutOfBounds, "PCI configuration write out of bounds", offset};
    }

    // Alignment checking
    if (width == PciAccessWidth::Word && (offset % 2 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 16-bit PCI configuration write",
                     offset};
    }
    if (width == PciAccessWidth::Dword && (offset % 4 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit PCI configuration write",
                     offset};
    }

    // Intercept BAR writes
    if (offset >= kPciBar0Offset && offset < kPciBar5Offset + 4) {
        const u8 bar_idx = (offset - kPciBar0Offset) / 4;
        const u8 bar_byte_offset = (offset - kPciBar0Offset) % 4;
        if (width == PciAccessWidth::Dword && bar_byte_offset == 0) {
            auto res = bars_[bar_idx].Write(value);
            if (res.has_value()) {
                NotifyChange();
            }
            return res;
        }
        // Partial BAR write
        u32 current_bar = bars_[bar_idx].Read();
        const u32 mask = (width == PciAccessWidth::Word) ? 0xFFFFu : 0xFFu;
        current_bar &= ~(mask << (bar_byte_offset * 8));
        current_bar |= ((value & mask) << (bar_byte_offset * 8));
        auto res = bars_[bar_idx].Write(current_bar);
        if (res.has_value()) {
            NotifyChange();
        }
        return res;
    }

    // Intercept Command writes (offset 0x04)
    if (offset == kPciCommandOffset && width == PciAccessWidth::Word) {
        WriteRaw(kPciCommandOffset, width, value);
        NotifyChange();
        return {};
    }
    if (offset >= kPciCommandOffset && offset < kPciCommandOffset + 2) {
        WriteRaw(offset, width, value);
        NotifyChange();
        return {};
    }

    // Intercept Status writes (offset 0x06): W1C bits
    if (offset == kPciStatusOffset && width == PciAccessWidth::Word) {
        const u16 current = status();
        const u16 w1c_mask = 0xF900; // Bits 15, 14, 13, 12, 11, 8
        const u16 new_val = (current & ~static_cast<u16>(value & w1c_mask));
        WriteRaw(kPciStatusOffset, width, new_val);
        return {};
    }

    // Read-only registers
    if (offset < kPciCommandOffset || // Vendor & Device ID
        (offset >= kPciRevisionIdOffset && offset < kPciCacheLineSizeOffset) ||       // Class/Rev
        offset == kPciHeaderTypeOffset ||                                             // Header type
        (offset >= kPciSubsystemVendorIdOffset && offset < kPciExpansionRomOffset)) { // Subsystem
        return {}; // Silently ignore writes to read-only fields per PCI specification
    }

    WriteRaw(offset, width, value);
    return {};
}

} // namespace xblob::pci
