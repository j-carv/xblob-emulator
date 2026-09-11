#include "xblob/pci/pci_bar.hpp"

#include "xblob/common/safe_math.hpp"

namespace xblob::pci {

PciBar::PciBar(u8 index, GuestSize size, bool is_io, bool is_prefetchable,
               std::shared_ptr<bus::BusDevice> device)
    : index_(index), size_(size), is_io_(is_io), is_prefetchable_(is_prefetchable),
      device_(std::move(device)) {
    if (size_ > 0) {
        const u32 flags = is_io_
                              ? kPciBarIoFlag
                              : (kPciBarMemoryFlag | (is_prefetchable_ ? kPciBarPrefetchable : 0));
        raw_value_ = flags;
    }
}

u32 PciBar::Read() const noexcept {
    if (!is_configured()) {
        return 0;
    }
    const u32 mask = is_io_ ? kPciBarIoAddressMask : kPciBarMemoryAddressMask;
    const u32 flags =
        is_io_ ? kPciBarIoFlag : (kPciBarMemoryFlag | (is_prefetchable_ ? kPciBarPrefetchable : 0));

    if (probe_mode_) {
        return (~(size_ - 1) & mask) | flags;
    }
    if (is_programmed_) {
        return (base_address_ & mask) | flags;
    }
    return flags;
}

Result<void> PciBar::ProgramAddress(GuestAddr addr) {
    if (!is_configured()) {
        return Error{ErrorCode::InvalidState, "Cannot program unconfigured BAR"};
    }
    // 32-bit address space overflow check
    if (static_cast<u64>(addr) + size_ > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow, "BAR range exceeds 32-bit address space", addr};
    }
    // Alignment check: address must be naturally aligned to BAR size
    if ((addr & (size_ - 1)) != 0) {
        return Error{ErrorCode::InvalidArgument, "BAR address misaligned relative to BAR size",
                     addr};
    }

    const u32 mask = is_io_ ? kPciBarIoAddressMask : kPciBarMemoryAddressMask;
    const u32 flags =
        is_io_ ? kPciBarIoFlag : (kPciBarMemoryFlag | (is_prefetchable_ ? kPciBarPrefetchable : 0));

    base_address_ = addr & mask;
    raw_value_ = (addr & mask) | flags;
    is_programmed_ = true;
    probe_mode_ = false;
    return {};
}

Result<void> PciBar::Write(u32 value) {
    if (!is_configured()) {
        return {};
    }
    if (value == 0xFFFFFFFFu) {
        probe_mode_ = true;
        return {};
    }

    const u32 mask = is_io_ ? kPciBarIoAddressMask : kPciBarMemoryAddressMask;
    const GuestAddr candidate_addr = value & mask;

    // Save previous state for transactional rollback
    const GuestAddr old_base = base_address_;
    const u32 old_raw = raw_value_;
    const bool old_programmed = is_programmed_;
    const bool old_probe = probe_mode_;

    auto result = ProgramAddress(candidate_addr);
    if (!result.has_value()) {
        // Rollback state
        base_address_ = old_base;
        raw_value_ = old_raw;
        is_programmed_ = old_programmed;
        probe_mode_ = old_probe;
        return result;
    }

    return {};
}

bool PciBar::Overlaps(GuestAddr other_base, GuestSize other_size) const noexcept {
    if (!is_active() || other_size == 0) {
        return false;
    }
    const u64 a_start = base_address_;
    const u64 a_end = static_cast<u64>(base_address_) + size_;
    const u64 b_start = other_base;
    const u64 b_end = static_cast<u64>(other_base) + other_size;
    return a_start < b_end && b_start < a_end;
}

} // namespace xblob::pci
