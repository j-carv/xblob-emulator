#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/pci/pci_device.hpp"
#include "xblob/pci/pci_types.hpp"

#include <map>
#include <memory>
#include <vector>

namespace xblob::pci {

class PciRegistry {
public:
    PciRegistry() = default;

    Result<void> RegisterDevice(std::shared_ptr<PciDevice> device);
    Result<void> UnregisterDevice(PciBdf bdf);

    [[nodiscard]] std::shared_ptr<PciDevice> FindDevice(PciBdf bdf) const noexcept;
    [[nodiscard]] std::size_t device_count() const noexcept { return devices_.size(); }
    [[nodiscard]] std::vector<std::shared_ptr<PciDevice>> EnumerateDevices() const;

    [[nodiscard]] Result<u32> ReadConfig(PciBdf bdf, u8 offset,
                                         PciAccessWidth width) const noexcept;
    [[nodiscard]] Result<void> WriteConfig(PciBdf bdf, u8 offset, PciAccessWidth width, u32 value);

private:
    std::map<PciBdf, std::shared_ptr<PciDevice>> devices_;
};

} // namespace xblob::pci
