#pragma once

#include "xblob/pci/pci_config_header.hpp"
#include "xblob/pci/pci_types.hpp"

#include <string_view>

namespace xblob::pci {

class PciDevice {
public:
    explicit PciDevice(PciBdf bdf) : bdf_(bdf) {}
    virtual ~PciDevice() = default;

    [[nodiscard]] PciBdf bdf() const noexcept { return bdf_; }
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] PciConfigHeader& config_header() noexcept { return config_header_; }
    [[nodiscard]] const PciConfigHeader& config_header() const noexcept { return config_header_; }

protected:
    PciBdf bdf_;
    PciConfigHeader config_header_;
};

} // namespace xblob::pci
