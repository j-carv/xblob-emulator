#pragma once

#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_types.hpp"

#include <cmath>
#include <cstddef>
#include <string_view>

namespace xblob::gpu {

enum class PrimitiveType : u32 {
    None = 0,
    Points = 1,
    Lines = 2,
    LineLoop = 3,
    LineStrip = 4,
    Triangles = 5,
    TriangleStrip = 6,
    TriangleFan = 7,
    Quads = 8,
    QuadStrip = 9,
    Polygon = 10,
};

enum class CullMode : u32 {
    None = 0,
    CW = 0x404,
    CCW = 0x405,
};

enum class DepthFunc : u32 {
    Never = 0x200,
    Less = 0x201,
    Equal = 0x202,
    LessEqual = 0x203,
    Greater = 0x204,
    NotEqual = 0x205,
    GreaterEqual = 0x206,
    Always = 0x207,
};

enum class BlendFactor : u32 {
    Zero = 0x0,
    One = 0x1,
    SrcColor = 0x300,
    InvSrcColor = 0x301,
    SrcAlpha = 0x302,
    InvSrcAlpha = 0x303,
    DstAlpha = 0x304,
    InvDstAlpha = 0x305,
    DstColor = 0x306,
    InvDstColor = 0x307,
};

enum class BlendEquation : u32 {
    Add = 0x8006,
    Min = 0x8007,
    Max = 0x8008,
    Subtract = 0x800A,
    ReverseSubtract = 0x800B,
};

enum class VertexFormat : u32 {
    None = 0,
    Float1 = 1,
    Float2 = 2,
    Float3 = 3,
    Float4 = 4,
    Ubyte4 = 5,
};

[[nodiscard]] constexpr u32 GetVertexFormatBytes(VertexFormat fmt) noexcept {
    switch (fmt) {
    case VertexFormat::None:
        return 0;
    case VertexFormat::Float1:
        return 4;
    case VertexFormat::Float2:
        return 8;
    case VertexFormat::Float3:
        return 12;
    case VertexFormat::Float4:
        return 16;
    case VertexFormat::Ubyte4:
        return 4;
    }
    return 0;
}

enum class IndexFormat : u32 {
    None = 0,
    Index16 = 1,
    Index32 = 2,
};

enum class TextureFormat : u32 {
    Unknown = 0,
    R8G8B8A8 = 1,
    A8R8G8B8 = 6,
    X8R8G8B8 = 12,
    R5G6B5 = 14,
};

[[nodiscard]] constexpr u32 GetTextureBytesPerPixel(TextureFormat fmt) noexcept {
    switch (fmt) {
    case TextureFormat::R8G8B8A8:
    case TextureFormat::A8R8G8B8:
    case TextureFormat::X8R8G8B8:
        return 4;
    case TextureFormat::R5G6B5:
        return 2;
    default:
        return 0;
    }
}

enum class TextureLayout : u32 {
    Swizzled = 0,
    Linear = 1,
};

enum class TextureAddressMode : u32 {
    Wrap = 1,
    Mirror = 2,
    ClampToEdge = 3,
    ClampToBorder = 4,
};

enum class TextureFilter : u32 {
    Nearest = 1,
    Linear = 2,
};

struct Viewport {
    float scale_x{1.0f};
    float scale_y{1.0f};
    float scale_z{1.0f};
    float scale_w{1.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
    float offset_z{0.0f};
    float offset_w{0.0f};
};

struct Scissor {
    u32 x{0};
    u32 y{0};
    u32 width{1920};
    u32 height{1080};
    bool enabled{false};
};

struct ColorMask {
    bool write_r{true};
    bool write_g{true};
    bool write_b{true};
    bool write_a{true};
};

struct VertexAttribute {
    GuestAddr address{0};
    u32 stride{0};
    VertexFormat format{VertexFormat::None};
    bool enabled{false};
};

struct IndexBufferBinding {
    GuestAddr address{0};
    IndexFormat format{IndexFormat::None};
};

struct TextureStage {
    GuestAddr address{0};
    u32 width{0};
    u32 height{0};
    u32 pitch{0};
    TextureFormat format{TextureFormat::Unknown};
    TextureLayout layout{TextureLayout::Linear};
    TextureAddressMode wrap_u{TextureAddressMode::ClampToEdge};
    TextureAddressMode wrap_v{TextureAddressMode::ClampToEdge};
    TextureFilter min_filter{TextureFilter::Nearest};
    TextureFilter mag_filter{TextureFilter::Nearest};
    bool enabled{false};
};

struct UnsupportedMethodEntry {
    u32 class_id{0};
    u32 method{0};
    u32 subchannel{0};
    u32 count{0};
    u32 parameter{0};
};

// Architecture & Engine Constants
constexpr u32 kMaxSubchannels = 8;
constexpr u32 kMaxVertexAttributes = 16;
constexpr u32 kMaxTextureStages = 4;
constexpr std::size_t kMaxUnsupportedDiagnosticEntries = 64;

constexpr u32 kClassNv097Kelvin3D = 0x0097;
constexpr u32 kClassNv096Kelvin3D = 0x0096;
constexpr u32 kClassNv062Surface = 0x0062;
constexpr u32 kClassNv01fBlit = 0x001F;
constexpr u32 kClassNv044Video = 0x0044;
constexpr u32 kClassSynthetic2D = 0x0000;

// Pushbuffer 3D Method Offsets
constexpr u32 kMethodSetObject = 0x0000;
constexpr u32 kMethod3dClearSurface = 0x01D0;
constexpr u32 kMethod3dClearColor = 0x01D4;
constexpr u32 kMethod3dClearDepth = 0x01D8;
constexpr u32 kMethod3dSurfaceClipHorizontal = 0x0200;
constexpr u32 kMethod3dSurfaceClipVertical = 0x0204;
constexpr u32 kMethod3dBlendEnable = 0x0320;
constexpr u32 kMethod3dBlendFuncSrc = 0x0324;
constexpr u32 kMethod3dBlendFuncDst = 0x0328;
constexpr u32 kMethod3dBlendEquation = 0x032C;
constexpr u32 kMethod3dColorMask = 0x0350;
constexpr u32 kMethod3dDepthMask = 0x0354;
constexpr u32 kMethod3dDepthFunc = 0x0358;
constexpr u32 kMethod3dDepthTestEnable = 0x035C;
constexpr u32 kMethod3dCullFace = 0x039C;
constexpr u32 kMethod3dCullFaceEnable = 0x03A0;
constexpr u32 kMethod3dScissorHorizontal = 0x08F8;
constexpr u32 kMethod3dScissorVertical = 0x08FC;
constexpr u32 kMethod3dViewportScaleX = 0x0A00;
constexpr u32 kMethod3dViewportScaleY = 0x0A04;
constexpr u32 kMethod3dViewportScaleZ = 0x0A08;
constexpr u32 kMethod3dViewportScaleW = 0x0A0C;
constexpr u32 kMethod3dViewportOffsetX = 0x0A20;
constexpr u32 kMethod3dViewportOffsetY = 0x0A24;
constexpr u32 kMethod3dViewportOffsetZ = 0x0A28;
constexpr u32 kMethod3dViewportOffsetW = 0x0A2C;
constexpr u32 kMethod3dVertexOffsetBase = 0x1680;
constexpr u32 kMethod3dIndexAddress = 0x16AC;
constexpr u32 kMethod3dVertexFormatBase = 0x1740;
constexpr u32 kMethod3dIndexFormat = 0x1760;
constexpr u32 kMethod3dBeginEnd = 0x17FC;
constexpr u32 kMethod3dDrawArrays = 0x1808;
constexpr u32 kMethod3dDrawElements32 = 0x1818;
constexpr u32 kMethod3dDrawElements16 = 0x181C;
constexpr u32 kMethod3dTextureOffsetBase = 0x1B00;
constexpr u32 kMethod3dTextureFormatBase = 0x1B04;
constexpr u32 kMethod3dTextureAddressBase = 0x1B08;
constexpr u32 kMethod3dTextureControlBase = 0x1B0C;
constexpr u32 kMethod3dTextureFilterBase = 0x1B10;
constexpr u32 kMethod3dTextureSizeBase = 0x1B14;

} // namespace xblob::gpu
