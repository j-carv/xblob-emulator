#pragma once

#include "xblob/common/types.hpp"

#include <cstddef>
#include <string_view>

namespace xblob::gpu {

enum class PixelFormat : u32 {
    Rgba8 = 1,
};

enum class GpuFault : u32 {
    None = 0,
    UnknownRegister = 1,
    MisalignedRegisterAccess = 2,
    ReadOnlyRegister = 3,
    PushbufferTruncated = 4,
    UnknownOpcode = 5,
    UnknownMethod = 6,
    InvalidSurfaceDimensions = 7,
    BudgetExhausted = 8,
    InvalidCoordinates = 9,
    UnsupportedTextureFormat = 10,
    InvalidMemoryAccess = 11,
    InvalidState = 12,
};

[[nodiscard]] constexpr std::string_view ToString(GpuFault fault) noexcept {
    switch (fault) {
    case GpuFault::None:
        return "None";
    case GpuFault::UnknownRegister:
        return "UnknownRegister";
    case GpuFault::MisalignedRegisterAccess:
        return "MisalignedRegisterAccess";
    case GpuFault::ReadOnlyRegister:
        return "ReadOnlyRegister";
    case GpuFault::PushbufferTruncated:
        return "PushbufferTruncated";
    case GpuFault::UnknownOpcode:
        return "UnknownOpcode";
    case GpuFault::UnknownMethod:
        return "UnknownMethod";
    case GpuFault::InvalidSurfaceDimensions:
        return "InvalidSurfaceDimensions";
    case GpuFault::BudgetExhausted:
        return "BudgetExhausted";
    case GpuFault::InvalidCoordinates:
        return "InvalidCoordinates";
    case GpuFault::UnsupportedTextureFormat:
        return "UnsupportedTextureFormat";
    case GpuFault::InvalidMemoryAccess:
        return "InvalidMemoryAccess";
    case GpuFault::InvalidState:
        return "InvalidState";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, GpuFault fault) {
    return os << ToString(fault);
}

struct GpuFrameMetadata {
    u32 width{0};
    u32 height{0};
    u32 pitch{0};
    PixelFormat pixel_format{PixelFormat::Rgba8};
    u64 sequence_number{0};
    u64 frame_cycle{0};
    u32 buffer_size{0};
};

// Safe conservative hardware limits
constexpr u32 kMaxSurfaceWidth = 1920;
constexpr u32 kMaxSurfaceHeight = 1080;
constexpr u32 kMaxSurfacePitch = 1920 * 4;
constexpr u32 kMaxSurfaceBytes = 1920 * 1080 * 4; // 8,294,400 bytes (~8.3 MB)

// Clean-Room Documented NV2A MMIO Register Offsets
constexpr u32 kRegPmcBoot0 = 0x00000000;
constexpr u32 kRegPmcEnable = 0x00000200;
constexpr u32 kRegPtimerTime0 = 0x00009400;
constexpr u32 kRegPtimerTime1 = 0x00009410;
constexpr u32 kRegPfifoPending = 0x00002100;
constexpr u32 kRegPfifoMask = 0x00002140;
constexpr u32 kRegPfifoRamHt = 0x00002210;
constexpr u32 kRegPfifoRamFc = 0x00002214;
constexpr u32 kRegPfifoRamRo = 0x00002218;
constexpr u32 kRegPfifoMode = 0x00002500;
constexpr u32 kRegPgraphIntr = 0x00400100;
constexpr u32 kRegPgraphIntrEn = 0x00400140;
constexpr u32 kRegPgraphStatus = 0x00400700;
constexpr u32 kRegPvideoIntr = 0x00680100;
constexpr u32 kRegPvideoIntrEn = 0x00680140;
constexpr u32 kRegPvideoBuffer = 0x00680800;

// Hardware identification constants
constexpr u32 kNv2aChipIdRevision = 0x02A000A1;

} // namespace xblob::gpu
