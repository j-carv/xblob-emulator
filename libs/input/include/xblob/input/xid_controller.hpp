#pragma once

#include "xblob/input/input_types.hpp"
#include "xblob/usb/usb_device.hpp"

#include <mutex>
#include <string_view>
#include <vector>

namespace xblob::input {

struct UnsupportedUsbRequest {
    u8 request_type{0};
    u8 request{0};
    u16 value{0};
    u16 index{0};
    u64 count{0};
};

class XidController final : public usb::UsbDevice {
public:
    explicit XidController(std::string_view name = "Xbox Controller");
    ~XidController() override = default;

    // UsbDevice interface
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] u8 address() const noexcept override { return address_; }
    void set_address(u8 address) noexcept override { address_ = address; }
    [[nodiscard]] usb::UsbSpeed speed() const noexcept override { return usb::UsbSpeed::FullSpeed; }
    [[nodiscard]] bool is_connected() const noexcept override;

    void Reset() noexcept override;

    [[nodiscard]] usb::UsbTransferResult
    HandleControlTransfer(const usb::UsbSetupPacket& setup, ByteSpan out_payload,
                          MutableByteSpan in_buffer) noexcept override;

    [[nodiscard]] usb::UsbTransferResult
    HandleInterruptTransfer(u8 endpoint_address, MutableByteSpan in_buffer) noexcept override;

    // Host input injection
    // Returns true if snapshot was accepted, false if stale (sequence <= last_sequence)
    [[nodiscard]] bool SubmitSnapshot(const HostInputSnapshot& snapshot) noexcept;

    // Hotplug management
    void Connect() noexcept;
    void Disconnect() noexcept;

    // Current report inspection
    [[nodiscard]] XidGamepadReport GetCurrentReport() const noexcept;
    [[nodiscard]] u64 current_sequence() const noexcept { return last_sequence_; }

    // Rumble inspection
    [[nodiscard]] u16 rumble_left_motor() const noexcept { return rumble_left_; }
    [[nodiscard]] u16 rumble_right_motor() const noexcept { return rumble_right_; }

    // Unsupported requests telemetry
    [[nodiscard]] u32 unsupported_requests_count() const noexcept;
    [[nodiscard]] std::vector<UnsupportedUsbRequest> unsupported_requests() const;

private:
    [[nodiscard]] usb::UsbTransferResult
    HandleGetDescriptor(u8 desc_type, u8 desc_index, u16 max_len, MutableByteSpan dest) noexcept;
    void RecordUnsupportedRequestLocked(u8 request_type, u8 request, u16 value, u16 index) noexcept;

    std::string name_;
    u8 address_{0};
    u8 configuration_{0};
    bool connected_{true};
    bool endpoint1_halted_{false};

    mutable std::mutex mutex_;
    u64 last_sequence_{0};
    XidGamepadReport current_report_{};

    u16 rumble_left_{0};
    u16 rumble_right_{0};
    std::vector<UnsupportedUsbRequest> unsupported_requests_{};
};

} // namespace xblob::input
