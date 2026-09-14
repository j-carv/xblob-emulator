#include "xblob/gpu/rasterizer.hpp"

#include "xblob/gpu/texture_sampler.hpp"

#include <algorithm>
#include <cmath>

namespace xblob::gpu {

namespace {

float BlendFactorValue(BlendFactor factor, float src_c, float src_a, float dst_c,
                       float dst_a) noexcept {
    switch (factor) {
    case BlendFactor::Zero:
        return 0.0f;
    case BlendFactor::One:
        return 1.0f;
    case BlendFactor::SrcColor:
        return src_c;
    case BlendFactor::InvSrcColor:
        return 1.0f - src_c;
    case BlendFactor::SrcAlpha:
        return src_a;
    case BlendFactor::InvSrcAlpha:
        return 1.0f - src_a;
    case BlendFactor::DstAlpha:
        return dst_a;
    case BlendFactor::InvDstAlpha:
        return 1.0f - dst_a;
    case BlendFactor::DstColor:
        return dst_c;
    case BlendFactor::InvDstColor:
        return 1.0f - dst_c;
    }
    return 1.0f;
}

} // namespace

bool Rasterizer::DepthPasses(DepthFunc func, float frag_z, float buffer_z) noexcept {
    constexpr float kEps = 1e-6f;
    switch (func) {
    case DepthFunc::Never:
        return false;
    case DepthFunc::Less:
        return frag_z < (buffer_z - kEps);
    case DepthFunc::Equal:
        return std::abs(frag_z - buffer_z) <= kEps;
    case DepthFunc::LessEqual:
        return frag_z <= (buffer_z + kEps);
    case DepthFunc::Greater:
        return frag_z > (buffer_z + kEps);
    case DepthFunc::NotEqual:
        return std::abs(frag_z - buffer_z) > kEps;
    case DepthFunc::GreaterEqual:
        return frag_z >= (buffer_z - kEps);
    case DepthFunc::Always:
        return true;
    }
    return true;
}

ColorRgba Rasterizer::BlendPixels(const ColorRgba& src, const ColorRgba& dst, BlendFactor src_f,
                                  BlendFactor dst_f, BlendEquation eq) noexcept {
    constexpr float sf = 1.0f / 255.0f;
    const float s_r = src.r * sf, s_g = src.g * sf, s_b = src.b * sf, s_a = src.a * sf;
    const float d_r = dst.r * sf, d_g = dst.g * sf, d_b = dst.b * sf, d_a = dst.a * sf;

    const float sr_val = BlendFactorValue(src_f, s_r, s_a, d_r, d_a);
    const float sg_val = BlendFactorValue(src_f, s_g, s_a, d_g, d_a);
    const float sb_val = BlendFactorValue(src_f, s_b, s_a, d_b, d_a);
    const float sa_val = BlendFactorValue(src_f, s_a, s_a, d_a, d_a);

    const float dr_val = BlendFactorValue(dst_f, s_r, s_a, d_r, d_a);
    const float dg_val = BlendFactorValue(dst_f, s_g, s_a, d_g, d_a);
    const float db_val = BlendFactorValue(dst_f, s_b, s_a, d_b, d_a);
    const float da_val = BlendFactorValue(dst_f, s_a, s_a, d_a, d_a);

    auto apply_eq = [](float s_term, float d_term, BlendEquation e) -> float {
        switch (e) {
        case BlendEquation::Add:
            return s_term + d_term;
        case BlendEquation::Subtract:
            return s_term - d_term;
        case BlendEquation::ReverseSubtract:
            return d_term - s_term;
        case BlendEquation::Min:
            return std::min(s_term, d_term);
        case BlendEquation::Max:
            return std::max(s_term, d_term);
        }
        return s_term + d_term;
    };

    const float out_r = std::clamp(apply_eq(s_r * sr_val, d_r * dr_val, eq), 0.0f, 1.0f);
    const float out_g = std::clamp(apply_eq(s_g * sg_val, d_g * dg_val, eq), 0.0f, 1.0f);
    const float out_b = std::clamp(apply_eq(s_b * sb_val, d_b * db_val, eq), 0.0f, 1.0f);
    const float out_a = std::clamp(apply_eq(s_a * sa_val, d_a * da_val, eq), 0.0f, 1.0f);

    return ColorRgba{
        static_cast<u8>(std::round(out_r * 255.0f)),
        static_cast<u8>(std::round(out_g * 255.0f)),
        static_cast<u8>(std::round(out_b * 255.0f)),
        static_cast<u8>(std::round(out_a * 255.0f)),
    };
}

Result<u32> Rasterizer::RasterizeTriangle(const Triangle& tri, const RasterizerState& state,
                                          GpuSurface& surface, std::vector<float>& depth_buffer,
                                          const PushbufferDecoder::ReadWordFn& reader) {
    const float area2 =
        (tri.v[1].screen_x - tri.v[0].screen_x) * (tri.v[2].screen_y - tri.v[0].screen_y) -
        (tri.v[1].screen_y - tri.v[0].screen_y) * (tri.v[2].screen_x - tri.v[0].screen_x);

    if (std::abs(area2) < 1e-6f) {
        return 0; // Degenerate triangle
    }

    if (state.cull_enabled) {
        if (state.cull_mode == CullMode::CW && area2 < 0.0f) {
            return 0; // Culled CW
        }
        if (state.cull_mode == CullMode::CCW && area2 > 0.0f) {
            return 0; // Culled CCW
        }
    }

    TransformedVertex va = tri.v[0];
    TransformedVertex vb = tri.v[1];
    TransformedVertex vc = tri.v[2];

    // Ensure counter-clockwise winding for rasterization equations
    if (area2 < 0.0f) {
        std::swap(vb, vc);
    }

    // 28.4 fixed point (scale by 16)
    const auto to_fixed = [](float val) -> i64 {
        return static_cast<i64>(std::round(val * 16.0f));
    };

    const i64 xa = to_fixed(va.screen_x);
    const i64 ya = to_fixed(va.screen_y);
    const i64 xb = to_fixed(vb.screen_x);
    const i64 yb = to_fixed(vb.screen_y);
    const i64 xc = to_fixed(vc.screen_x);
    const i64 yc = to_fixed(vc.screen_y);

    const i64 delta_fixed = (xb - xa) * (yc - ya) - (yb - ya) * (xc - xa);
    if (delta_fixed <= 0) {
        return 0;
    }

    // Top-left rule checks: edge is top-left if horizontal and pointing right, or if going up
    const bool is_tl_bc = ((yc - yb) == 0 && (xc - xb) > 0) || ((yc - yb) < 0);
    const bool is_tl_ca = ((ya - yc) == 0 && (xa - xc) > 0) || ((ya - yc) < 0);
    const bool is_tl_ab = ((yb - ya) == 0 && (xb - xa) > 0) || ((yb - ya) < 0);

    int min_x = std::max(
        0, static_cast<int>(std::floor(std::min({va.screen_x, vb.screen_x, vc.screen_x}))));
    int max_x =
        std::min(static_cast<int>(surface.width() - 1),
                 static_cast<int>(std::ceil(std::max({va.screen_x, vb.screen_x, vc.screen_x}))));
    int min_y = std::max(
        0, static_cast<int>(std::floor(std::min({va.screen_y, vb.screen_y, vc.screen_y}))));
    int max_y =
        std::min(static_cast<int>(surface.height() - 1),
                 static_cast<int>(std::ceil(std::max({va.screen_y, vb.screen_y, vc.screen_y}))));

    if (state.scissor.enabled) {
        min_x = std::max(min_x, static_cast<int>(state.scissor.x));
        max_x = std::min(max_x, static_cast<int>(state.scissor.x + state.scissor.width - 1));
        min_y = std::max(min_y, static_cast<int>(state.scissor.y));
        max_y = std::min(max_y, static_cast<int>(state.scissor.y + state.scissor.height - 1));
    }

    if (min_x > max_x || min_y > max_y) {
        return 0;
    }

    u32 pixels_rasterized = 0;
    const double inv_delta = 1.0 / static_cast<double>(delta_fixed);

    for (int y = min_y; y <= max_y; ++y) {
        const i64 cur_y = static_cast<i64>(y) * 16 + 8;
        const std::size_t row_depth_offset = static_cast<std::size_t>(y) * surface.width();

        for (int x = min_x; x <= max_x; ++x) {
            const i64 cur_x = static_cast<i64>(x) * 16 + 8;

            const i64 e_bc = (xc - xb) * (cur_y - yb) - (yc - yb) * (cur_x - xb);
            const i64 e_ca = (xa - xc) * (cur_y - yc) - (ya - yc) * (cur_x - xc);
            const i64 e_ab = (xb - xa) * (cur_y - ya) - (yb - ya) * (cur_x - xa);

            const bool pass_bc = is_tl_bc ? (e_bc >= 0) : (e_bc > 0);
            const bool pass_ca = is_tl_ca ? (e_ca >= 0) : (e_ca > 0);
            const bool pass_ab = is_tl_ab ? (e_ab >= 0) : (e_ab > 0);

            if (!pass_bc || !pass_ca || !pass_ab) {
                continue;
            }

            const double wa = static_cast<double>(e_bc) * inv_delta;
            const double wb = static_cast<double>(e_ca) * inv_delta;
            const double wc = static_cast<double>(e_ab) * inv_delta;

            const float z =
                static_cast<float>(wa * va.screen_z + wb * vb.screen_z + wc * vc.screen_z);
            const std::size_t depth_idx = row_depth_offset + static_cast<std::size_t>(x);

            if (state.depth_test_enabled) {
                if (!DepthPasses(state.depth_func, z, depth_buffer[depth_idx])) {
                    continue;
                }
            }

            // Perspective-correct attributes
            const float inv_w = static_cast<float>(wa * va.inv_w + wb * vb.inv_w + wc * vc.inv_w);
            const float w = (inv_w > 1e-9f) ? (1.0f / inv_w) : 1.0f;

            const float r =
                static_cast<float>((wa * va.color.r * va.inv_w + wb * vb.color.r * vb.inv_w +
                                    wc * vc.color.r * vc.inv_w) *
                                   w);
            const float g =
                static_cast<float>((wa * va.color.g * va.inv_w + wb * vb.color.g * vb.inv_w +
                                    wc * vc.color.g * vc.inv_w) *
                                   w);
            const float b =
                static_cast<float>((wa * va.color.b * va.inv_w + wb * vb.color.b * vb.inv_w +
                                    wc * vc.color.b * vc.inv_w) *
                                   w);
            const float a =
                static_cast<float>((wa * va.color.a * va.inv_w + wb * vb.color.a * vb.inv_w +
                                    wc * vc.color.a * vc.inv_w) *
                                   w);

            ColorRgba frag{
                static_cast<u8>(std::clamp(std::round(r), 0.0f, 255.0f)),
                static_cast<u8>(std::clamp(std::round(g), 0.0f, 255.0f)),
                static_cast<u8>(std::clamp(std::round(b), 0.0f, 255.0f)),
                static_cast<u8>(std::clamp(std::round(a), 0.0f, 255.0f)),
            };

            if (state.texture_stage0.enabled) {
                const float u = static_cast<float>(
                    (wa * va.u * va.inv_w + wb * vb.u * vb.inv_w + wc * vc.u * vc.inv_w) * w);
                const float v = static_cast<float>(
                    (wa * va.v * va.inv_w + wb * vb.v * vb.inv_w + wc * vc.v * vc.inv_w) * w);

                auto tex_res = TextureSampler::SampleNearest(state.texture_stage0, u, v, reader);
                if (!tex_res.has_value()) {
                    return tex_res.error();
                }
                const ColorRgba tex_col = *tex_res;
                frag.r = static_cast<u8>((static_cast<u32>(frag.r) * tex_col.r) / 255);
                frag.g = static_cast<u8>((static_cast<u32>(frag.g) * tex_col.g) / 255);
                frag.b = static_cast<u8>((static_cast<u32>(frag.b) * tex_col.b) / 255);
                frag.a = static_cast<u8>((static_cast<u32>(frag.a) * tex_col.a) / 255);
            }

            auto cur_px_res = surface.GetPixel(static_cast<u32>(x), static_cast<u32>(y));
            if (!cur_px_res.has_value()) {
                continue;
            }
            const ColorRgba dst = ColorRgba::FromU32(*cur_px_res);

            if (state.blend_enabled) {
                frag =
                    BlendPixels(frag, dst, state.src_blend, state.dst_blend, state.blend_equation);
            }

            const u8 out_r = state.color_mask.write_r ? frag.r : dst.r;
            const u8 out_g = state.color_mask.write_g ? frag.g : dst.g;
            const u8 out_b = state.color_mask.write_b ? frag.b : dst.b;
            const u8 out_a = state.color_mask.write_a ? frag.a : dst.a;

            (void)surface.SetPixel(static_cast<u32>(x), static_cast<u32>(y), out_r, out_g, out_b,
                                   out_a);

            if (state.depth_write_enabled) {
                depth_buffer[depth_idx] = z;
            }

            ++pixels_rasterized;
        }
    }

    return pixels_rasterized;
}

} // namespace xblob::gpu
