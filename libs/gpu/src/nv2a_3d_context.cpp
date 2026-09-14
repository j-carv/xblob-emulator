#include "xblob/gpu/nv2a_3d_context.hpp"

#include <algorithm>

namespace xblob::gpu {

namespace {
static const VertexAttribute kEmptyVertexAttr{};
static const TextureStage kEmptyTextureStage{};
} // namespace

Nv2a3dContext::Nv2a3dContext() {
    EnsureDepthBuffer(640, 480);
    unsupported_methods_.reserve(kMaxUnsupportedDiagnosticEntries);
}

void Nv2a3dContext::Reset() noexcept {
    viewport_ = Viewport{};
    scissor_ = Scissor{};
    cull_mode_ = CullMode::None;
    cull_enabled_ = false;
    depth_func_ = DepthFunc::LessEqual;
    depth_test_enabled_ = false;
    depth_write_enabled_ = true;
    clear_depth_value_ = 1.0f;

    color_mask_ = ColorMask{};
    clear_color_value_ = 0x00000000u;

    blend_enabled_ = false;
    blend_src_ = BlendFactor::One;
    blend_dst_ = BlendFactor::Zero;
    blend_equation_ = BlendEquation::Add;

    primitive_type_ = PrimitiveType::Triangles;
    vertex_attributes_.fill(VertexAttribute{});
    index_buffer_ = IndexBufferBinding{};
    texture_stages_.fill(TextureStage{});

    EnsureDepthBuffer(640, 480);
    ClearDepth(clear_depth_value_);

    ClearUnsupportedMethods();
}

const VertexAttribute& Nv2a3dContext::vertex_attribute(u32 idx) const noexcept {
    if (idx >= kMaxVertexAttributes) {
        return kEmptyVertexAttr;
    }
    return vertex_attributes_[idx];
}

void Nv2a3dContext::SetVertexAttribute(u32 idx, const VertexAttribute& attr) noexcept {
    if (idx < kMaxVertexAttributes) {
        vertex_attributes_[idx] = attr;
    }
}

const TextureStage& Nv2a3dContext::texture_stage(u32 idx) const noexcept {
    if (idx >= kMaxTextureStages) {
        return kEmptyTextureStage;
    }
    return texture_stages_[idx];
}

void Nv2a3dContext::SetTextureStage(u32 idx, const TextureStage& stage) noexcept {
    if (idx < kMaxTextureStages) {
        texture_stages_[idx] = stage;
    }
}

void Nv2a3dContext::EnsureDepthBuffer(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return;
    }
    if (width != depth_width_ || height != depth_height_ || depth_buffer_.empty()) {
        depth_width_ = width;
        depth_height_ = height;
        depth_buffer_.assign(static_cast<std::size_t>(width) * height, clear_depth_value_);
    }
}

float Nv2a3dContext::GetDepthPixel(u32 x, u32 y) const noexcept {
    if (x >= depth_width_ || y >= depth_height_ || depth_buffer_.empty()) {
        return 1.0f;
    }
    return depth_buffer_[static_cast<std::size_t>(y) * depth_width_ + x];
}

void Nv2a3dContext::ClearColor(GpuSurface& surface, u8 r, u8 g, u8 b, u8 a) {
    surface.Clear(r, g, b, a);
}

void Nv2a3dContext::ClearDepth(float depth_value) {
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), depth_value);
}

void Nv2a3dContext::ClearSurface(GpuSurface& surface, u32 flags) {
    if ((flags & 0x01) != 0) {
        const u8 r = static_cast<u8>(clear_color_value_ & 0xFF);
        const u8 g = static_cast<u8>((clear_color_value_ >> 8) & 0xFF);
        const u8 b = static_cast<u8>((clear_color_value_ >> 16) & 0xFF);
        const u8 a = static_cast<u8>((clear_color_value_ >> 24) & 0xFF);
        ClearColor(surface, r, g, b, a);
    }
    if ((flags & 0x02) != 0) {
        ClearDepth(clear_depth_value_);
    }
}

Result<void> Nv2a3dContext::ValidateDraw(u32 start, u32 count, bool indexed,
                                         const PushbufferDecoder::ReadWordFn& reader) const {
    if (primitive_type_ == PrimitiveType::Triangles) {
        if (count < 3 || (count % 3) != 0) {
            return Error{ErrorCode::InvalidArgument,
                         "TriangleList vertex count must be multiple of 3"};
        }
    } else if (primitive_type_ == PrimitiveType::TriangleStrip ||
               primitive_type_ == PrimitiveType::TriangleFan) {
        if (count < 3) {
            return Error{ErrorCode::InvalidArgument,
                         "Triangle strip/fan vertex count must be >= 3"};
        }
    } else {
        return Error{ErrorCode::UnsupportedFeature, "Unsupported primitive type for 3D draw",
                     static_cast<u64>(primitive_type_)};
    }

    auto fetch_val = VertexFetcher::ValidateFetch(vertex_attributes_, index_buffer_, start, count,
                                                  indexed, 65536, reader);
    if (!fetch_val.has_value()) {
        return fetch_val;
    }

    if (texture_stages_[0].enabled) {
        auto tex_val = TextureSampler::ValidateStage(texture_stages_[0]);
        if (!tex_val.has_value()) {
            return tex_val;
        }
    }

    return {};
}

Result<u32> Nv2a3dContext::Draw(u32 start, u32 count, bool indexed, GpuSurface& surface,
                                const PushbufferDecoder::ReadWordFn& reader) {
    EnsureDepthBuffer(surface.width(), surface.height());

    std::vector<Triangle> triangles;
    auto fetch_res =
        VertexFetcher::FetchAndAssemble(primitive_type_, vertex_attributes_, index_buffer_, start,
                                        count, indexed, viewport_, reader, triangles);
    if (!fetch_res.has_value()) {
        return fetch_res.error();
    }

    RasterizerState state;
    state.cull_mode = cull_mode_;
    state.cull_enabled = cull_enabled_;
    state.scissor = scissor_;
    state.depth_func = depth_func_;
    state.depth_test_enabled = depth_test_enabled_;
    state.depth_write_enabled = depth_write_enabled_;
    state.color_mask = color_mask_;
    state.blend_enabled = blend_enabled_;
    state.src_blend = blend_src_;
    state.dst_blend = blend_dst_;
    state.blend_equation = blend_equation_;
    state.texture_stage0 = texture_stages_[0];

    for (const auto& tri : triangles) {
        auto rast_res = Rasterizer::RasterizeTriangle(tri, state, surface, depth_buffer_, reader);
        if (!rast_res.has_value()) {
            return rast_res.error();
        }
    }

    return static_cast<u32>(triangles.size());
}

void Nv2a3dContext::RecordUnsupportedMethod(u32 class_id, u32 method, u32 subchannel, u32 count,
                                            u32 param) noexcept {
    if (unsupported_methods_.size() < kMaxUnsupportedDiagnosticEntries) {
        unsupported_methods_.push_back(
            UnsupportedMethodEntry{class_id, method, subchannel, count, param});
    }
    ++total_unsupported_methods_count_;
}

} // namespace xblob::gpu
