#pragma once

#include "xblob/bus/bus.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/pci/pci_registry.hpp"

#include <memory>
#include <vector>

namespace xblob::pci {

class PciBusBridge {
public:
    PciBusBridge(bus::Bus& bus, PciRegistry& registry);
    ~PciBusBridge();

    [[nodiscard]] Result<void> SyncDevice(PciBdf bdf);
    [[nodiscard]] Result<void> SyncAll();

    [[nodiscard]] Result<u32> ReadConfig(PciBdf bdf, u8 offset,
                                         PciAccessWidth width) const noexcept;
    [[nodiscard]] Result<void> WriteConfig(PciBdf bdf, u8 offset, PciAccessWidth width, u32 value);

    [[nodiscard]] Result<void> ProgramBar(PciBdf bdf, u8 bar_idx, GuestAddr new_addr);
    [[nodiscard]] Result<void> SetMemorySpaceEnabled(PciBdf bdf, bool enabled);

private:
    struct ActiveMapping {
        PciBdf bdf;
        u8 bar_idx;
        GuestAddr base;
        GuestSize size;
    };

    [[nodiscard]] bool HasCollision(PciBdf bdf, u8 bar_idx, GuestAddr base,
                                    GuestSize size) const noexcept;
    Result<void> UnmapBar(PciBdf bdf, u8 bar_idx);
    Result<void> MapBar(PciBdf bdf, u8 bar_idx);

    bus::Bus& bus_;
    PciRegistry& registry_;
    std::vector<ActiveMapping> active_mappings_;
};

} // namespace xblob::pci
