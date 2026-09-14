#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_surface.hpp"
#include "xblob/gpu/nv2a_3d_types.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/vertex_fetch.hpp"

#include <vector>

namespace xblob::gpu {

struct RasterizerState {
    CullMode cull_mode{CullMode::None};
    bool cull_enabled{false};
    Scissor scissor;
    DepthFunc depth_func{DepthFunc::LessEqual};
    bool depth_test_enabled{false};
    bool depth_write_enabled{true};
    ColorMask color_mask;
    bool blend_enabled{false};
    BlendFactor src_blend{BlendFactor::One};
    BlendFactor dst_blend{BlendFactor::Zero};
    BlendEquation blend_equation{BlendEquation::Add};
    TextureStage texture_stage0;
};

class Rasterizer {
public:
    static Result<u32> RasterizeTriangle(const Triangle& tri, const RasterizerState& state,
                                         GpuSurface& surface, std::vector<float>& depth_buffer,
                                         const PushbufferDecoder::ReadWordFn& reader);

    static bool DepthPasses(DepthFunc func, float frag_z, float buffer_z) noexcept;

    static ColorRgba BlendPixels(const ColorRgba& src, const ColorRgba& dst, BlendFactor src_f,
                                 BlendFactor dst_f, BlendEquation eq) noexcept;
};

} // namespace xblob::gpu
