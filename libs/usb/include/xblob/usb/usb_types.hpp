#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <cstdint>
#include <string_view>

namespace xblob::usb {

enum class UsbSpeed : u8 {
    FullSpeed = 0,
    LowSpeed = 1,
};

enum class UsbPid : u8 {
    Setup = 0,
    Out = 1,
    In = 2,
    Reserved = 3,
};

enum class UsbTransferStatus : u8 {
    Success = 0,
    CrcError = 1,
    BitStuffing = 2,
    DataToggleMismatch = 3,
    Stalled = 4,
    DeviceNotResponding = 5,
    PidCheckFailure = 6,
    UnexpectedPid = 7,
    DataOverrun = 8,
    DataUnderrun = 9,
    BufferOverrun = 10,
    BufferUnderrun = 11,
    NotAccessed = 14,
    TraversalLimitExceeded = 15,
    InvalidDescriptor = 16,
    MemoryFault = 17,
    DeviceDisconnected = 18,
};

[[nodiscard]] constexpr std::string_view
UsbTransferStatusToString(UsbTransferStatus status) noexcept {
    switch (status) {
    case UsbTransferStatus::Success:
        return "Success";
    case UsbTransferStatus::CrcError:
        return "CrcError";
    case UsbTransferStatus::BitStuffing:
        return "BitStuffing";
    case UsbTransferStatus::DataToggleMismatch:
        return "DataToggleMismatch";
    case UsbTransferStatus::Stalled:
        return "Stalled";
    case UsbTransferStatus::DeviceNotResponding:
        return "DeviceNotResponding";
    case UsbTransferStatus::PidCheckFailure:
        return "PidCheckFailure";
    case UsbTransferStatus::UnexpectedPid:
        return "UnexpectedPid";
    case UsbTransferStatus::DataOverrun:
        return "DataOverrun";
    case UsbTransferStatus::DataUnderrun:
        return "DataUnderrun";
    case UsbTransferStatus::BufferOverrun:
        return "BufferOverrun";
    case UsbTransferStatus::BufferUnderrun:
        return "BufferUnderrun";
    case UsbTransferStatus::NotAccessed:
        return "NotAccessed";
    case UsbTransferStatus::TraversalLimitExceeded:
        return "TraversalLimitExceeded";
    case UsbTransferStatus::InvalidDescriptor:
        return "InvalidDescriptor";
    case UsbTransferStatus::MemoryFault:
        return "MemoryFault";
    case UsbTransferStatus::DeviceDisconnected:
        return "DeviceDisconnected";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, UsbTransferStatus status) {
    return os << UsbTransferStatusToString(status);
}

struct UsbTransferResult {
    UsbTransferStatus status{UsbTransferStatus::Success};
    u32 bytes_transferred{0};
    u8 condition_code{0};

    [[nodiscard]] bool ok() const noexcept { return status == UsbTransferStatus::Success; }
};

struct UsbSetupPacket {
    u8 request_type{0};
    u8 request{0};
    u16 value{0};
    u16 index{0};
    u16 length{0};

    [[nodiscard]] static Result<UsbSetupPacket> FromBytes(ByteSpan bytes) noexcept {
        if (bytes.size() < 8) {
            return Error{ErrorCode::TruncatedData, "USB Setup packet precisa de ao menos 8 bytes",
                         0};
        }
        UsbSetupPacket pkt;
        pkt.request_type = bytes[0];
        pkt.request = bytes[1];
        pkt.value = static_cast<u16>(bytes[2] | (static_cast<u16>(bytes[3]) << 8));
        pkt.index = static_cast<u16>(bytes[4] | (static_cast<u16>(bytes[5]) << 8));
        pkt.length = static_cast<u16>(bytes[6] | (static_cast<u16>(bytes[7]) << 8));
        return pkt;
    }

    [[nodiscard]] bool is_device_to_host() const noexcept { return (request_type & 0x80) != 0; }

    [[nodiscard]] u8 type() const noexcept { return static_cast<u8>((request_type >> 5) & 0x03); }

    [[nodiscard]] u8 recipient() const noexcept { return static_cast<u8>(request_type & 0x1F); }
};

namespace requests {
inline constexpr u8 kGetStatus = 0x00;
inline constexpr u8 kClearFeature = 0x01;
inline constexpr u8 kSetFeature = 0x03;
inline constexpr u8 kSetAddress = 0x05;
inline constexpr u8 kGetDescriptor = 0x06;
inline constexpr u8 kSetDescriptor = 0x07;
inline constexpr u8 kGetConfiguration = 0x08;
inline constexpr u8 kSetConfiguration = 0x09;
inline constexpr u8 kGetInterface = 0x0A;
inline constexpr u8 kSetInterface = 0x0B;
} // namespace requests

namespace descriptors {
inline constexpr u8 kDevice = 0x01;
inline constexpr u8 kConfiguration = 0x02;
inline constexpr u8 kString = 0x03;
inline constexpr u8 kInterface = 0x04;
inline constexpr u8 kEndpoint = 0x05;
inline constexpr u8 kXid = 0x42;
} // namespace descriptors

} // namespace xblob::usb
