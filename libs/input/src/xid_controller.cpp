#include "xblob/input/xid_controller.hpp"

#include <algorithm>
#include <cstring>

namespace xblob::input {

namespace {

// Standard USB Device Descriptor for Xbox Duke / Controller S
inline constexpr u8 kDeviceDescriptor[18] = {
    18,         // bLength
    0x01,       // bDescriptorType (DEVICE)
    0x10, 0x01, // bcdUSB (1.10)
    0x00,       // bDeviceClass
    0x00,       // bDeviceSubClass
    0x00,       // bDeviceProtocol
    0x08,       // bMaxPacketSize0 (8 bytes)
    0x5E, 0x04, // idVendor (0x045E - Microsoft)
    0x89, 0x02, // idProduct (0x0289 - Xbox Controller S)
    0x00, 0x01, // bcdDevice (1.00)
    0x00,       // iManufacturer
    0x00,       // iProduct
    0x00,       // iSerialNumber
    0x01        // bNumConfigurations
};

// Full Configuration Descriptor (Configuration + Interface + 2 Endpoints)
inline constexpr u8 kConfigurationDescriptor[32] = {
    // Configuration Descriptor (9 bytes)
    9,        // bLength
    0x02,     // bDescriptorType (CONFIGURATION)
    32, 0x00, // wTotalLength (32 bytes)
    0x01,     // bNumInterfaces
    0x01,     // bConfigurationValue
    0x00,     // iConfiguration
    0x80,     // bmAttributes (Bus Powered)
    0x32,     // bMaxPower (100 mA)

    // Interface Descriptor (9 bytes)
    9,    // bLength
    0x04, // bDescriptorType (INTERFACE)
    0x00, // bInterfaceNumber
    0x00, // bAlternateSetting
    0x02, // bNumEndpoints
    0x58, // bInterfaceClass (Xbox Controller)
    0x42, // bInterfaceSubClass
    0x00, // bInterfaceProtocol
    0x00, // iInterface

    // Endpoint 1 Descriptor: Interrupt IN (7 bytes)
    7,          // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x81,       // bEndpointAddress (EP 1 IN)
    0x03,       // bmAttributes (Interrupt)
    0x20, 0x00, // wMaxPacketSize (32 bytes)
    0x04,       // bInterval (4 ms)

    // Endpoint 2 Descriptor: Interrupt OUT (7 bytes)
    7,          // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x02,       // bEndpointAddress (EP 2 OUT - Rumble)
    0x03,       // bmAttributes (Interrupt)
    0x20, 0x00, // wMaxPacketSize (32 bytes)
    0x04        // bInterval (4 ms)
};

// XID Capabilities Descriptor (16 bytes)
inline constexpr u8 kXidDescriptor[16] = {
    16,                          // bLength
    0x42,                        // bDescriptorType (XID Descriptor)
    0x00, 0x01,                  // bcdXid (1.00)
    0x01,                        // bType (Gamepad)
    0x01,                        // bSubType (Duke / S-pad)
    0x14,                        // bMaxInputReportSize (20 bytes)
    0x06,                        // bMaxOutputReportSize (6 bytes)
    0xFF, 0xFF,                  // wButtons (All digital buttons supported)
    0xFF,                        // bAnalogButtons
    0x00, 0x00, 0x00, 0x00, 0x00 // Reserved padding
};

} // namespace

XidController::XidController(std::string_view name) : name_(name) {
    Reset();
}

bool XidController::is_connected() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

void XidController::Reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    address_ = 0;
    configuration_ = 0;
    endpoint1_halted_ = false;
    current_report_ = XidGamepadReport{};
    rumble_left_ = 0;
    rumble_right_ = 0;
}

void XidController::Connect() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = true;
}

void XidController::Disconnect() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
}

XidGamepadReport XidController::GetCurrentReport() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_report_;
}

bool XidController::SubmitSnapshot(const HostInputSnapshot& snapshot) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // Enforce monotonic sequence: drop stale or duplicate snapshots
    if (snapshot.sequence <= last_sequence_) {
        return false;
    }

    last_sequence_ = snapshot.sequence;
    connected_ = snapshot.connected;

    auto normalized = snapshot;
    normalized.ClampAndNormalize();
    current_report_ = XidGamepadReport::FromSnapshot(normalized);
    return true;
}

usb::UsbTransferResult XidController::HandleGetDescriptor(u8 desc_type,
                                                          [[maybe_unused]] u8 desc_index,
                                                          u16 max_len,
                                                          MutableByteSpan dest) const noexcept {

    const u8* src_data = nullptr;
    u32 src_len = 0;

    switch (desc_type) {
    case usb::descriptors::kDevice:
        src_data = kDeviceDescriptor;
        src_len = sizeof(kDeviceDescriptor);
        break;
    case usb::descriptors::kConfiguration:
        src_data = kConfigurationDescriptor;
        src_len = sizeof(kConfigurationDescriptor);
        break;
    case usb::descriptors::kXid:
        src_data = kXidDescriptor;
        src_len = sizeof(kXidDescriptor);
        break;
    default:
        return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
    }

    u32 to_copy = std::min({static_cast<u32>(dest.size()), static_cast<u32>(max_len), src_len});
    std::memcpy(dest.data(), src_data, to_copy);
    return usb::UsbTransferResult{usb::UsbTransferStatus::Success, to_copy, 0};
}

usb::UsbTransferResult XidController::HandleControlTransfer(const usb::UsbSetupPacket& setup,
                                                            ByteSpan out_payload,
                                                            MutableByteSpan in_buffer) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!connected_) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::DeviceDisconnected, 0, 5};
    }

    u8 req_type = setup.type();
    u8 req = setup.request;

    if (req_type == 0) {
        // Standard USB request
        switch (req) {
        case usb::requests::kGetDescriptor: {
            u8 desc_type = static_cast<u8>((setup.value >> 8) & 0xFF);
            u8 desc_idx = static_cast<u8>(setup.value & 0xFF);
            return HandleGetDescriptor(desc_type, desc_idx, setup.length, in_buffer);
        }
        case usb::requests::kSetAddress:
            address_ = static_cast<u8>(setup.value & 0x7F);
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kGetConfiguration:
            if (!in_buffer.empty()) {
                in_buffer[0] = configuration_;
                return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 1, 0};
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kSetConfiguration:
            configuration_ = static_cast<u8>(setup.value & 0xFF);
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kGetStatus:
            if (in_buffer.size() >= 2) {
                in_buffer[0] = (setup.recipient() == 2 && endpoint1_halted_) ? 0x01 : 0x00;
                in_buffer[1] = 0x00;
                return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 2, 0};
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kClearFeature:
            if (setup.recipient() == 2 && setup.value == 0) { // ENDPOINT_HALT
                endpoint1_halted_ = false;
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kSetFeature:
            if (setup.recipient() == 2 && setup.value == 0) { // ENDPOINT_HALT
                endpoint1_halted_ = true;
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kGetInterface:
            if (!in_buffer.empty()) {
                in_buffer[0] = 0;
                return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 1, 0};
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        case usb::requests::kSetInterface:
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        default:
            return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
        }
    } else if (req_type == 1 || req_type == 2) {
        // Class / Vendor request (XID specific)
        if (req == 0x01) {
            // GET_CAPABILITIES
            return HandleGetDescriptor(usb::descriptors::kXid, 0, setup.length, in_buffer);
        }
        if (req == 0x06) {
            // GET_DESCRIPTOR XID
            return HandleGetDescriptor(usb::descriptors::kXid, 0, setup.length, in_buffer);
        }
        if (req == 0x09) {
            // SET_REPORT (Rumble motors)
            if (out_payload.size() >= 6) {
                // Byte 0: 0x00, Byte 1: 0x06, Bytes 2..3: Left rumble, Bytes 4..5: Right rumble
                rumble_left_ =
                    static_cast<u16>(out_payload[2]) | (static_cast<u16>(out_payload[3]) << 8);
                rumble_right_ =
                    static_cast<u16>(out_payload[4]) | (static_cast<u16>(out_payload[5]) << 8);
                return usb::UsbTransferResult{usb::UsbTransferStatus::Success,
                                              static_cast<u32>(out_payload.size()), 0};
            }
            return usb::UsbTransferResult{usb::UsbTransferStatus::Success, 0, 0};
        }
        return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
    }

    return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
}

usb::UsbTransferResult XidController::HandleInterruptTransfer(u8 endpoint_address,
                                                              MutableByteSpan in_buffer) noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!connected_) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::DeviceDisconnected, 0, 5};
    }

    u8 ep_num = endpoint_address & 0x0F;
    if (ep_num != 1) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
    }

    if (endpoint1_halted_) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::Stalled, 0, 4};
    }

    if (in_buffer.size() < kXidGamepadReportSize) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::BufferUnderrun, 0, 11};
    }

    auto ser_res = current_report_.Serialize(in_buffer);
    if (!ser_res) {
        return usb::UsbTransferResult{usb::UsbTransferStatus::MemoryFault, 0, 17};
    }

    return usb::UsbTransferResult{usb::UsbTransferStatus::Success, kXidGamepadReportSize, 0};
}

} // namespace xblob::input
