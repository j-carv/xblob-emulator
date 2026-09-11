#pragma once

#include "xblob/bus/device.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/pci/pci_types.hpp"

#include <memory>

namespace xblob::pci {

class PciBar {
public:
    PciBar() = default;
    PciBar(u8 index, GuestSize size, bool is_io = false, bool is_prefetchable = false,
           std::shared_ptr<bus::BusDevice> device = nullptr);

    [[nodiscard]] u8 index() const noexcept { return index_; }
    [[nodiscard]] GuestSize size() const noexcept { return size_; }
    [[nodiscard]] bool is_io() const noexcept { return is_io_; }
    [[nodiscard]] bool is_prefetchable() const noexcept { return is_prefetchable_; }
    [[nodiscard]] bool is_configured() const noexcept { return size_ > 0; }
    [[nodiscard]] bool is_active() const noexcept { return is_configured() && is_programmed_; }
    [[nodiscard]] GuestAddr base_address() const noexcept { return base_address_; }
    [[nodiscard]] u32 raw_value() const noexcept { return raw_value_; }
    [[nodiscard]] std::shared_ptr<bus::BusDevice> device() const noexcept { return device_; }

    void SetDevice(std::shared_ptr<bus::BusDevice> device) noexcept { device_ = std::move(device); }

    [[nodiscard]] u32 Read() const noexcept;
    [[nodiscard]] Result<void> Write(u32 value);
    [[nodiscard]] Result<void> ProgramAddress(GuestAddr addr);

    [[nodiscard]] bool Overlaps(GuestAddr other_base, GuestSize other_size) const noexcept;

private:
    u8 index_{0};
    GuestSize size_{0};
    bool is_io_{false};
    bool is_prefetchable_{false};
    bool is_programmed_{false};
    bool probe_mode_{false};
    GuestAddr base_address_{0};
    u32 raw_value_{0};
    std::shared_ptr<bus::BusDevice> device_{nullptr};
};

} // namespace xblob::pci
