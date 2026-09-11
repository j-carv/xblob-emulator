#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/pci/pci_bar.hpp"
#include "xblob/pci/pci_types.hpp"

#include <array>
#include <functional>

namespace xblob::pci {

class PciConfigHeader {
public:
    using ChangeCallback = std::function<void()>;

    PciConfigHeader();

    // Configuration init helpers
    void SetVendorId(u16 vendor_id) noexcept;
    void SetDeviceId(u16 device_id) noexcept;
    void SetClassCodes(u8 base_class, u8 subclass, u8 prog_if) noexcept;
    void SetRevisionId(u8 revision_id) noexcept;
    void SetSubsystemIds(u16 sub_vendor_id, u16 sub_device_id) noexcept;
    void SetInterrupt(u8 line, u8 pin) noexcept;

    Result<void> ConfigureBar(u8 index, GuestSize size, bool is_io = false,
                              bool is_prefetchable = false,
                              std::shared_ptr<bus::BusDevice> device = nullptr);

    [[nodiscard]] PciBar& bar(u8 index);
    [[nodiscard]] const PciBar& bar(u8 index) const;

    [[nodiscard]] u16 vendor_id() const noexcept;
    [[nodiscard]] u16 device_id() const noexcept;
    [[nodiscard]] u16 command() const noexcept;
    [[nodiscard]] u16 status() const noexcept;
    [[nodiscard]] bool is_memory_space_enabled() const noexcept;
    [[nodiscard]] bool is_io_space_enabled() const noexcept;

    void SetChangeCallback(ChangeCallback callback) noexcept {
        change_callback_ = std::move(callback);
    }

    [[nodiscard]] Result<u32> Read(u8 offset, PciAccessWidth width) const noexcept;
    [[nodiscard]] Result<void> Write(u8 offset, PciAccessWidth width, u32 value);

private:
    void NotifyChange() noexcept;
    [[nodiscard]] u32 ReadRaw(u8 offset, PciAccessWidth width) const noexcept;
    void WriteRaw(u8 offset, PciAccessWidth width, u32 value) noexcept;

    std::array<u8, kPciConfigSpaceSize> raw_bytes_{};
    std::array<PciBar, kPciBarCount> bars_{};
    ChangeCallback change_callback_{nullptr};
};

} // namespace xblob::pci
