#pragma once

#include "xblob/bus/device.hpp"
#include "xblob/common/error.hpp"
#include "xblob/usb/ohci_controller.hpp"

namespace xblob::machine {

class OhciBusDevice final : public bus::BusDevice {
public:
    explicit OhciBusDevice(usb::OhciController& ohci) : ohci_(ohci) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "OHCI USB Controller"; }

    [[nodiscard]] Result<u32> Read(u32 offset, bus::BusAccessWidth width) override {
        if (width != bus::BusAccessWidth::Dword) {
            return Error{ErrorCode::InvalidArgument,
                         "OHCI MMIO suporta exclusivamente acessos de 32 bits (Dword)"};
        }
        return ohci_.ReadRegister(offset);
    }

    [[nodiscard]] Result<void> Write(u32 offset, bus::BusAccessWidth width, u32 value) override {
        if (width != bus::BusAccessWidth::Dword) {
            return Error{ErrorCode::InvalidArgument,
                         "OHCI MMIO suporta exclusivamente acessos de 32 bits (Dword)"};
        }
        return ohci_.WriteRegister(offset, value);
    }

private:
    usb::OhciController& ohci_;
};

} // namespace xblob::machine
