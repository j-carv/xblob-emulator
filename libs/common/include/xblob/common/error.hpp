#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace xblob {

enum class ErrorCode {
    Ok = 0,

    // I/O errors
    FileNotFound,
    AccessDenied,
    IoError,
    UnexpectedEof,

    // Structural / Parsing errors
    InvalidMagic,
    UnknownFormat,
    TruncatedData,
    IntegerOverflow,
    OutOfBounds,
    InvalidHeader,
    InvalidField,

    // Feature support
    UnsupportedFormat,
    UnsupportedFeature,

    // Memory faults / errors
    UnmappedAddress,
    AccessViolation,
    RegionOverlap,
    InvalidCapacity,
    UnsupportedAccessSize,

    // Scheduler errors
    PastCycle,
    EventNotFound,
    LimitReached,

    // CPU faults / status
    InvalidOpcode,
    GeneralProtection,
    SegmentNotPresent,
    ExecutionFault,
    CpuHalted,

    // Application / Generic
    InvalidArgument,
    InvalidState,
    InternalError
};

[[nodiscard]] std::string_view ErrorCodeToString(ErrorCode code) noexcept;

inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    return os << ErrorCodeToString(code);
}

struct Error {
    ErrorCode code{ErrorCode::Ok};
    std::string message{};
    std::uint64_t offset{0};

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }

    [[nodiscard]] explicit operator bool() const noexcept { return code != ErrorCode::Ok; }
};

} // namespace xblob
