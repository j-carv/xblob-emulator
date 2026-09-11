#include "xblob/pci/pci_registry.hpp"

namespace xblob::pci {

Result<void> PciRegistry::RegisterDevice(std::shared_ptr<PciDevice> device) {
    if (!device) {
        return Error{ErrorCode::InvalidArgument, "Cannot register null PCI device"};
    }
    const auto bdf = device->bdf();
    if (devices_.find(bdf) != devices_.end()) {
        return Error{ErrorCode::RegionOverlap, "Duplicate PCI BDF registration"};
    }
    devices_[bdf] = std::move(device);
    return {};
}

Result<void> PciRegistry::UnregisterDevice(PciBdf bdf) {
    auto it = devices_.find(bdf);
    if (it == devices_.end()) {
        return Error{ErrorCode::InvalidArgument, "PCI device not found to unregister"};
    }
    devices_.erase(it);
    return {};
}

std::shared_ptr<PciDevice> PciRegistry::FindDevice(PciBdf bdf) const noexcept {
    auto it = devices_.find(bdf);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<PciDevice>> PciRegistry::EnumerateDevices() const {
    std::vector<std::shared_ptr<PciDevice>> list;
    list.reserve(devices_.size());
    for (const auto& [bdf, dev] : devices_) {
        list.push_back(dev);
    }
    return list;
}

Result<u32> PciRegistry::ReadConfig(PciBdf bdf, u8 offset, PciAccessWidth width) const noexcept {
    // Check alignment even for absent devices
    if (width == PciAccessWidth::Word && (offset % 2 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 16-bit PCI configuration read",
                     offset};
    }
    if (width == PciAccessWidth::Dword && (offset % 4 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit PCI configuration read",
                     offset};
    }

    auto dev = FindDevice(bdf);
    if (!dev) {
        // Absent device: return all-ones without fault
        switch (width) {
        case PciAccessWidth::Byte:
            return 0xFFu;
        case PciAccessWidth::Word:
            return 0xFFFFu;
        case PciAccessWidth::Dword:
            return 0xFFFFFFFFu;
        }
    }

    return dev->config_header().Read(offset, width);
}

Result<void> PciRegistry::WriteConfig(PciBdf bdf, u8 offset, PciAccessWidth width, u32 value) {
    // Check alignment
    if (width == PciAccessWidth::Word && (offset % 2 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 16-bit PCI configuration write",
                     offset};
    }
    if (width == PciAccessWidth::Dword && (offset % 4 != 0)) {
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit PCI configuration write",
                     offset};
    }

    auto dev = FindDevice(bdf);
    if (!dev) {
        // Absent device write dropped silently
        return {};
    }

    return dev->config_header().Write(offset, width, value);
}

} // namespace xblob::pci
