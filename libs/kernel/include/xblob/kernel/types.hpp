#pragma once

#include "xblob/common/types.hpp"

#include <cstdint>

namespace xblob::kernel {

using GuestHandle = u32;
using ThreadId = u32;

constexpr GuestHandle kInvalidHandle = 0xFFFFFFFF;
constexpr ThreadId kInvalidThreadId = 0;

// Standard NT status codes (public specification values)
constexpr u32 kStatusSuccess = 0x00000000;
constexpr u32 kStatusWaitTimeout = 0x00000102;
constexpr u32 kStatusUnsuccessful = 0xC0000001;
constexpr u32 kStatusAccessViolation = 0xC0000005;
constexpr u32 kStatusInvalidHandle = 0xC0000008;
constexpr u32 kStatusInvalidParameter = 0xC000000D;
constexpr u32 kStatusNoMemory = 0xC0000017;
constexpr u32 kStatusNotImplemented = 0xC0000002;

enum class HandleType : u8 {
    None = 0,
    Event = 1,
    Mutex = 2,
    Thread = 3,
    Semaphore = 4,
    Timer = 5,
    File = 6,
};

// Additional NT status codes
constexpr u32 kStatusWait0 = 0x00000000;
constexpr u32 kStatusAlerted = 0x00000101;
constexpr u32 kStatusConflict = 0xC0000018;
constexpr u32 kStatusBufferTooSmall = 0xC0000023;
constexpr u32 kStatusMutantNotOwned = 0xC0000046;
constexpr u32 kStatusSemaphoreLimitExceeded = 0xC0000047;

struct HandleEntry {
    HandleType type{HandleType::None};
    u16 generation{1};
    u32 object_id{0};
};

[[nodiscard]] constexpr GuestHandle EncodeHandle(u16 index, u16 generation) noexcept {
    return (static_cast<u32>(generation) << 16) | static_cast<u32>(index);
}

[[nodiscard]] constexpr u16 GetHandleIndex(GuestHandle handle) noexcept {
    return static_cast<u16>(handle & 0xFFFF);
}

[[nodiscard]] constexpr u16 GetHandleGeneration(GuestHandle handle) noexcept {
    return static_cast<u16>((handle >> 16) & 0xFFFF);
}

} // namespace xblob::kernel
