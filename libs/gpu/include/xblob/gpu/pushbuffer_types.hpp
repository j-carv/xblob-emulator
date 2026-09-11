#pragma once

#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_types.hpp"

#include <vector>

namespace xblob::gpu {

enum class PacketOpcode : u8 {
    Method = 0,
    Jump = 1,
    Call = 2,
    Return = 3,
};

inline std::ostream& operator<<(std::ostream& os, PacketOpcode op) {
    switch (op) {
    case PacketOpcode::Method:
        return os << "Method";
    case PacketOpcode::Jump:
        return os << "Jump";
    case PacketOpcode::Call:
        return os << "Call";
    case PacketOpcode::Return:
        return os << "Return";
    }
    return os << "Unknown";
}

// Allowlisted Synthetic NV2A Pushbuffer Methods
constexpr u32 kMethodNop = 0x0000;
constexpr u32 kMethodSurfaceWidth = 0x0100;
constexpr u32 kMethodSurfaceHeight = 0x0104;
constexpr u32 kMethodSurfacePitch = 0x0108;
constexpr u32 kMethodClearColor = 0x0120;
constexpr u32 kMethodClearSurface = 0x0124;
constexpr u32 kMethodRectX = 0x0140;
constexpr u32 kMethodRectY = 0x0144;
constexpr u32 kMethodRectW = 0x0148;
constexpr u32 kMethodRectH = 0x014C;
constexpr u32 kMethodRectColor = 0x0150;
constexpr u32 kMethodRectDraw = 0x0154;
constexpr u32 kMethodFlip = 0x0180;

struct PushbufferPacket {
    PacketOpcode opcode{PacketOpcode::Method};
    u32 method{0};
    u32 count{0};
    bool non_incrementing{false};
    GuestAddr jump_target{0};
    std::vector<u32> parameters;
};

struct PushbufferBudgets {
    u32 max_words{65536};
    u32 max_packets{4096};
    u32 max_methods{16384};
    u32 max_jumps{64};
};

struct PushbufferStats {
    u32 words_processed{0};
    u32 packets_processed{0};
    u32 methods_executed{0};
    u32 jumps_taken{0};
};

} // namespace xblob::gpu
