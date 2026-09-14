#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/nv2a_3d_types.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"

namespace xblob::gpu {

struct ColorRgba {
    u8 r{255};
    u8 g{255};
    u8 b{255};
    u8 a{255};

    [[nodiscard]] constexpr u32 ToU32() const noexcept {
        return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) |
               (static_cast<u32>(a) << 24);
    }

    [[nodiscard]] static constexpr ColorRgba FromU32(u32 val) noexcept {
        return ColorRgba{
            static_cast<u8>(val & 0xFF),
            static_cast<u8>((val >> 8) & 0xFF),
            static_cast<u8>((val >> 16) & 0xFF),
            static_cast<u8>((val >> 24) & 0xFF),
        };
    }
};

class TextureSampler {
public:
    static Result<void> ValidateStage(const TextureStage& stage);

    static Result<ColorRgba> SampleNearest(const TextureStage& stage, float u, float v,
                                           const PushbufferDecoder::ReadWordFn& reader);

    static u32 ComputeSwizzleOffset(u32 x, u32 y, u32 width, u32 height) noexcept;
};

} // namespace xblob::gpu
