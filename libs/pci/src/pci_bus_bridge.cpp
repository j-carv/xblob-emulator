#include "xblob/pci/pci_bus_bridge.hpp"

#include "xblob/common/safe_math.hpp"

#include <algorithm>

namespace xblob::pci {

PciBusBridge::PciBusBridge(bus::Bus& bus, PciRegistry& registry) : bus_(bus), registry_(registry) {}

PciBusBridge::~PciBusBridge() {
    for (const auto& mapping : active_mappings_) {
        (void)bus_.UnmapDevice(mapping.base);
    }
    active_mappings_.clear();
}

bool PciBusBridge::HasCollision(PciBdf bdf, u8 bar_idx, GuestAddr base,
                                GuestSize size) const noexcept {
    if (size == 0) {
        return false;
    }
    const u64 req_start = base;
    const u64 req_end = static_cast<u64>(base) + size;

    // Check against other devices in registry
    for (const auto& dev : registry_.EnumerateDevices()) {
        for (u8 i = 0; i < kPciBarCount; ++i) {
            if (dev->bdf() == bdf && i == bar_idx) {
                continue;
            }
            const auto& other_bar = dev->config_header().bar(i);
            if (other_bar.is_active()) {
                const u64 o_start = other_bar.base_address();
                const u64 o_end = static_cast<u64>(other_bar.base_address()) + other_bar.size();
                if (req_start < o_end && o_start < req_end) {
                    return true;
                }
            }
        }
    }

    // Check against bus mappings that don't belong to this BAR
    for (const auto& mapping : bus_.mappings()) {
        bool is_self = false;
        for (const auto& act : active_mappings_) {
            if (act.bdf == bdf && act.bar_idx == bar_idx && act.base == mapping.base) {
                is_self = true;
                break;
            }
        }
        if (!is_self && mapping.Overlaps(base, size)) {
            return true;
        }
    }

    return false;
}

Result<void> PciBusBridge::UnmapBar(PciBdf bdf, u8 bar_idx) {
    auto it =
        std::find_if(active_mappings_.begin(), active_mappings_.end(),
                     [&](const ActiveMapping& m) { return m.bdf == bdf && m.bar_idx == bar_idx; });
    if (it != active_mappings_.end()) {
        auto res = bus_.UnmapDevice(it->base);
        active_mappings_.erase(it);
        return res;
    }
    return {};
}

Result<void> PciBusBridge::MapBar(PciBdf bdf, u8 bar_idx) {
    auto dev = registry_.FindDevice(bdf);
    if (!dev) {
        return Error{ErrorCode::InvalidArgument, "PCI device not found for MapBar"};
    }
    auto& bar = dev->config_header().bar(bar_idx);
    if (!bar.is_active() || !bar.device() || bar.is_io()) {
        return {};
    }
    if (!dev->config_header().is_memory_space_enabled()) {
        return {};
    }

    // Check if already mapped
    auto it =
        std::find_if(active_mappings_.begin(), active_mappings_.end(),
                     [&](const ActiveMapping& m) { return m.bdf == bdf && m.bar_idx == bar_idx; });
    if (it != active_mappings_.end()) {
        if (it->base == bar.base_address() && it->size == bar.size()) {
            return {};
        }
        (void)UnmapBar(bdf, bar_idx);
    }

    auto res = bus_.MapDevice(bar.base_address(), bar.size(), bar.device());
    if (res.has_value()) {
        active_mappings_.push_back({bdf, bar_idx, bar.base_address(), bar.size()});
    }
    return res;
}

Result<void> PciBusBridge::ProgramBar(PciBdf bdf, u8 bar_idx, GuestAddr new_addr) {
    auto dev = registry_.FindDevice(bdf);
    if (!dev) {
        return Error{ErrorCode::InvalidArgument, "PCI device not found"};
    }
    if (bar_idx >= kPciBarCount) {
        return Error{ErrorCode::OutOfBounds, "PCI BAR index out of bounds"};
    }
    auto& bar = dev->config_header().bar(bar_idx);
    if (!bar.is_configured()) {
        return Error{ErrorCode::InvalidState, "PCI BAR not configured"};
    }

    // Check overflow
    if (static_cast<u64>(new_addr) + bar.size() > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow, "BAR range exceeds 32-bit address space",
                     new_addr};
    }
    // Check alignment
    if ((new_addr & (bar.size() - 1)) != 0) {
        return Error{ErrorCode::InvalidArgument, "BAR address misaligned", new_addr};
    }

    // Check collision against all other mappings
    if (HasCollision(bdf, bar_idx, new_addr, bar.size())) {
        return Error{ErrorCode::RegionOverlap, "BAR address range collides with existing region",
                     new_addr};
    }

    const GuestAddr old_addr = bar.base_address();
    const bool was_active = bar.is_active();
    const bool was_mapped =
        std::any_of(active_mappings_.begin(), active_mappings_.end(),
                    [&](const ActiveMapping& m) { return m.bdf == bdf && m.bar_idx == bar_idx; });

    // Unmap old location if currently mapped
    if (was_mapped) {
        (void)UnmapBar(bdf, bar_idx);
    }

    auto prog_res = bar.ProgramAddress(new_addr);
    if (!prog_res.has_value()) {
        // Rollback
        if (was_mapped && was_active) {
            (void)bar.ProgramAddress(old_addr);
            (void)MapBar(bdf, bar_idx);
        }
        return prog_res;
    }

    // If memory space is enabled, map new address to bus
    if (dev->config_header().is_memory_space_enabled() && bar.device() && !bar.is_io()) {
        auto map_res = MapBar(bdf, bar_idx);
        if (!map_res.has_value()) {
            // Rollback
            (void)bar.ProgramAddress(old_addr);
            if (was_mapped && was_active) {
                (void)MapBar(bdf, bar_idx);
            }
            return map_res;
        }
    }

    return {};
}

Result<void> PciBusBridge::SetMemorySpaceEnabled(PciBdf bdf, bool enabled) {
    auto dev = registry_.FindDevice(bdf);
    if (!dev) {
        return Error{ErrorCode::InvalidArgument, "PCI device not found"};
    }

    u16 cmd = dev->config_header().command();
    if (enabled) {
        cmd |= kPciCommandMemorySpace;
    } else {
        cmd &= ~kPciCommandMemorySpace;
    }

    (void)dev->config_header().Write(kPciCommandOffset, PciAccessWidth::Word, cmd);

    if (enabled) {
        for (u8 i = 0; i < kPciBarCount; ++i) {
            (void)MapBar(bdf, i);
        }
    } else {
        for (u8 i = 0; i < kPciBarCount; ++i) {
            (void)UnmapBar(bdf, i);
        }
    }
    return {};
}

Result<void> PciBusBridge::SyncDevice(PciBdf bdf) {
    auto dev = registry_.FindDevice(bdf);
    if (!dev) {
        return {};
    }
    if (dev->config_header().is_memory_space_enabled()) {
        for (u8 i = 0; i < kPciBarCount; ++i) {
            (void)MapBar(bdf, i);
        }
    } else {
        for (u8 i = 0; i < kPciBarCount; ++i) {
            (void)UnmapBar(bdf, i);
        }
    }
    return {};
}

Result<void> PciBusBridge::SyncAll() {
    for (const auto& dev : registry_.EnumerateDevices()) {
        (void)SyncDevice(dev->bdf());
    }
    return {};
}

Result<u32> PciBusBridge::ReadConfig(PciBdf bdf, u8 offset, PciAccessWidth width) const noexcept {
    return registry_.ReadConfig(bdf, offset, width);
}

Result<void> PciBusBridge::WriteConfig(PciBdf bdf, u8 offset, PciAccessWidth width, u32 value) {
    // If writing to BAR address
    if (offset >= kPciBar0Offset && offset < kPciBar5Offset + 4) {
        if (value == 0xFFFFFFFFu) {
            // Sizing probe
            return registry_.WriteConfig(bdf, offset, width, value);
        }
        const u8 bar_idx = (offset - kPciBar0Offset) / 4;
        const u8 byte_offset = (offset - kPciBar0Offset) % 4;
        if (width == PciAccessWidth::Dword && byte_offset == 0) {
            return ProgramBar(bdf, bar_idx, value & kPciBarMemoryAddressMask);
        }
    }

    // If writing to Command register (offset 0x04)
    if (offset == kPciCommandOffset && width == PciAccessWidth::Word) {
        const bool mem_enabled = (value & kPciCommandMemorySpace) != 0;
        return SetMemorySpaceEnabled(bdf, mem_enabled);
    }

    auto res = registry_.WriteConfig(bdf, offset, width, value);
    if (res.has_value()) {
        (void)SyncDevice(bdf);
    }
    return res;
}

} // namespace xblob::pci
