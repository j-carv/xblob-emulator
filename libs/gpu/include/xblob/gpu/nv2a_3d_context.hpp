#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_surface.hpp"
#include "xblob/gpu/nv2a_3d_types.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/rasterizer.hpp"
#include "xblob/gpu/vertex_fetch.hpp"

#include <array>
#include <vector>

namespace xblob::gpu {

class Nv2a3dContext {
public:
    Nv2a3dContext();

    void Reset() noexcept;

    // Viewport & Scissor
    [[nodiscard]] const Viewport& viewport() const noexcept { return viewport_; }
    Viewport& mutable_viewport() noexcept { return viewport_; }
    [[nodiscard]] const Scissor& scissor() const noexcept { return scissor_; }
    Scissor& mutable_scissor() noexcept { return scissor_; }

    // Cull & Depth
    [[nodiscard]] CullMode cull_mode() const noexcept { return cull_mode_; }
    void SetCullMode(CullMode mode) noexcept { cull_mode_ = mode; }
    [[nodiscard]] bool cull_enabled() const noexcept { return cull_enabled_; }
    void SetCullEnabled(bool en) noexcept { cull_enabled_ = en; }

    [[nodiscard]] DepthFunc depth_func() const noexcept { return depth_func_; }
    void SetDepthFunc(DepthFunc func) noexcept { depth_func_ = func; }
    [[nodiscard]] bool depth_test_enabled() const noexcept { return depth_test_enabled_; }
    void SetDepthTestEnabled(bool en) noexcept { depth_test_enabled_ = en; }
    [[nodiscard]] bool depth_write_enabled() const noexcept { return depth_write_enabled_; }
    void SetDepthWriteEnabled(bool en) noexcept { depth_write_enabled_ = en; }
    [[nodiscard]] float clear_depth_value() const noexcept { return clear_depth_value_; }
    void SetClearDepthValue(float d) noexcept { clear_depth_value_ = d; }

    // Color Mask & Blend
    [[nodiscard]] const ColorMask& color_mask() const noexcept { return color_mask_; }
    void SetColorMask(const ColorMask& m) noexcept { color_mask_ = m; }
    [[nodiscard]] u32 clear_color_value() const noexcept { return clear_color_value_; }
    void SetClearColorValue(u32 col) noexcept { clear_color_value_ = col; }

    [[nodiscard]] bool blend_enabled() const noexcept { return blend_enabled_; }
    void SetBlendEnabled(bool en) noexcept { blend_enabled_ = en; }
    [[nodiscard]] BlendFactor blend_src() const noexcept { return blend_src_; }
    void SetBlendSrc(BlendFactor sf) noexcept { blend_src_ = sf; }
    [[nodiscard]] BlendFactor blend_dst() const noexcept { return blend_dst_; }
    void SetBlendDst(BlendFactor df) noexcept { blend_dst_ = df; }
    [[nodiscard]] BlendEquation blend_equation() const noexcept { return blend_equation_; }
    void SetBlendEquation(BlendEquation eq) noexcept { blend_equation_ = eq; }

    // Primitive & Vertex / Index / Texture Bindings
    [[nodiscard]] PrimitiveType primitive_type() const noexcept { return primitive_type_; }
    void SetPrimitiveType(PrimitiveType pt) noexcept { primitive_type_ = pt; }

    [[nodiscard]] const VertexAttribute& vertex_attribute(u32 idx) const noexcept;
    void SetVertexAttribute(u32 idx, const VertexAttribute& attr) noexcept;

    [[nodiscard]] const IndexBufferBinding& index_buffer() const noexcept { return index_buffer_; }
    void SetIndexBuffer(const IndexBufferBinding& binding) noexcept { index_buffer_ = binding; }

    [[nodiscard]] const TextureStage& texture_stage(u32 idx) const noexcept;
    void SetTextureStage(u32 idx, const TextureStage& stage) noexcept;

    // Depth buffer management
    void EnsureDepthBuffer(u32 width, u32 height);
    [[nodiscard]] const std::vector<float>& depth_buffer() const noexcept { return depth_buffer_; }
    [[nodiscard]] float GetDepthPixel(u32 x, u32 y) const noexcept;

    // Clear
    void ClearSurface(GpuSurface& surface, u32 flags);
    void ClearColor(GpuSurface& surface, u8 r, u8 g, u8 b, u8 a);
    void ClearDepth(float depth_value);

    // Draw Operations & Atomic Validation
    Result<void> ValidateDraw(u32 start, u32 count, bool indexed,
                              const PushbufferDecoder::ReadWordFn& reader) const;

    Result<u32> Draw(u32 start, u32 count, bool indexed, GpuSurface& surface,
                     const PushbufferDecoder::ReadWordFn& reader);

    // Unsupported diagnostics
    void RecordUnsupportedMethod(u32 class_id, u32 method, u32 subchannel, u32 count,
                                 u32 param) noexcept;
    [[nodiscard]] const std::vector<UnsupportedMethodEntry>& unsupported_methods() const noexcept {
        return unsupported_methods_;
    }
    [[nodiscard]] u32 total_unsupported_methods_count() const noexcept {
        return total_unsupported_methods_count_;
    }
    void ClearUnsupportedMethods() noexcept {
        unsupported_methods_.clear();
        total_unsupported_methods_count_ = 0;
    }

private:
    Viewport viewport_;
    Scissor scissor_;
    CullMode cull_mode_{CullMode::None};
    bool cull_enabled_{false};
    DepthFunc depth_func_{DepthFunc::LessEqual};
    bool depth_test_enabled_{false};
    bool depth_write_enabled_{true};
    float clear_depth_value_{1.0f};

    ColorMask color_mask_;
    u32 clear_color_value_{0x00000000u};

    bool blend_enabled_{false};
    BlendFactor blend_src_{BlendFactor::One};
    BlendFactor blend_dst_{BlendFactor::Zero};
    BlendEquation blend_equation_{BlendEquation::Add};

    PrimitiveType primitive_type_{PrimitiveType::Triangles};
    std::array<VertexAttribute, kMaxVertexAttributes> vertex_attributes_{};
    IndexBufferBinding index_buffer_{};
    std::array<TextureStage, kMaxTextureStages> texture_stages_{};

    std::vector<float> depth_buffer_;
    u32 depth_width_{640};
    u32 depth_height_{480};

    std::vector<UnsupportedMethodEntry> unsupported_methods_;
    u32 total_unsupported_methods_count_{0};
};

} // namespace xblob::gpu
