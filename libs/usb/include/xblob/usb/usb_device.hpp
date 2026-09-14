#pragma once

#include "xblob/common/types.hpp"
#include "xblob/usb/usb_types.hpp"

#include <memory>
#include <string_view>

namespace xblob::usb {

class UsbDevice {
public:
    virtual ~UsbDevice() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual u8 address() const noexcept = 0;
    virtual void set_address(u8 address) noexcept = 0;
    [[nodiscard]] virtual UsbSpeed speed() const noexcept = 0;
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;

    virtual void Reset() noexcept = 0;

    [[nodiscard]] virtual UsbTransferResult
    HandleControlTransfer(const UsbSetupPacket& setup, ByteSpan out_payload,
                          MutableByteSpan in_buffer) noexcept = 0;

    [[nodiscard]] virtual UsbTransferResult
    HandleInterruptTransfer(u8 endpoint_address, MutableByteSpan in_buffer) noexcept = 0;
};

} // namespace xblob::usb
