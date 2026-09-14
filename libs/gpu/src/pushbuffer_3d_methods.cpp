#include "pushbuffer_3d_methods.hpp"

#include <cstring>

namespace xblob::gpu::detail {

namespace {

VertexFormat DecodeVertexFormat(u32 type, u32 count) {
    if (type == 2) {
        if (count == 1)
            return VertexFormat::Float1;
        if (count == 2)
            return VertexFormat::Float2;
        if (count == 3)
            return VertexFormat::Float3;
        if (count == 4)
            return VertexFormat::Float4;
    } else if (type == 4 && count == 4) {
        return VertexFormat::Ubyte4;
    }
    return VertexFormat::None;
}

float U32ToFloat(u32 val) noexcept {
    float f = 0.0f;
    std::memcpy(&f, &val, sizeof(float));
    return f;
}

} // namespace

ColorMask DecodeColorMask(u32 val) noexcept {
    ColorMask m;
    if (val > 0x0F) {
        m.write_b = (val & 0x00000001u) != 0;
        m.write_g = (val & 0x00000100u) != 0;
        m.write_r = (val & 0x00010000u) != 0;
        m.write_a = (val & 0x01000000u) != 0;
    } else {
        m.write_r = (val & 0x01u) != 0;
        m.write_g = (val & 0x02u) != 0;
        m.write_b = (val & 0x04u) != 0;
        m.write_a = (val & 0x08u) != 0;
    }
    return m;
}

void ApplyTextureStageMethod(u32 method, u32 val, Nv2a3dContext& ctx) noexcept {
    const u32 stage_idx = (method - kMethod3dTextureOffsetBase) / 0x40;
    const u32 sub_offset = (method - kMethod3dTextureOffsetBase) % 0x40;
    if (stage_idx >= kMaxTextureStages) {
        return;
    }
    auto stage = ctx.texture_stage(stage_idx);
    switch (sub_offset) {
    case 0x00:
        stage.address = val;
        break;
    case 0x04:
        stage.layout = ((val & 1) != 0) ? TextureLayout::Linear : TextureLayout::Swizzled;
        stage.format = static_cast<TextureFormat>((val >> 8) & 0xFFu);
        break;
    case 0x08:
        stage.wrap_u = static_cast<TextureAddressMode>(val & 0x0Fu);
        stage.wrap_v = static_cast<TextureAddressMode>((val >> 4) & 0x0Fu);
        break;
    case 0x0C:
        stage.enabled = ((val >> 30) & 0x01u) != 0;
        break;
    case 0x10:
        stage.min_filter = static_cast<TextureFilter>(val & 0x0Fu);
        stage.mag_filter = static_cast<TextureFilter>((val >> 4) & 0x0Fu);
        break;
    case 0x14:
        stage.width = val & 0xFFFFu;
        stage.height = (val >> 16) & 0xFFFFu;
        stage.pitch = stage.width * GetTextureBytesPerPixel(stage.format);
        break;
    default:
        break;
    }
    ctx.SetTextureStage(stage_idx, stage);
}

Result<void> Validate3dMethod(u32 method, u32 val, u32 count, u32 subchannel, u32 class_id,
                              Nv2a3dContext& ctx,
                              const PushbufferDecoder::ReadWordFn& memory_reader) {
    switch (method) {
    case kMethod3dClearSurface:
    case kMethod3dClearColor:
    case kMethod3dClearDepth:
    case kMethod3dSurfaceClipHorizontal:
    case kMethod3dSurfaceClipVertical:
    case kMethod3dScissorHorizontal:
    case kMethod3dScissorVertical:
    case kMethod3dViewportScaleX:
    case kMethod3dViewportScaleY:
    case kMethod3dViewportScaleZ:
    case kMethod3dViewportScaleW:
    case kMethod3dViewportOffsetX:
    case kMethod3dViewportOffsetY:
    case kMethod3dViewportOffsetZ:
    case kMethod3dViewportOffsetW:
    case kMethod3dBlendEnable:
    case kMethod3dBlendFuncSrc:
    case kMethod3dBlendFuncDst:
    case kMethod3dBlendEquation:
    case kMethod3dColorMask:
    case kMethod3dDepthMask:
    case kMethod3dDepthFunc:
    case kMethod3dDepthTestEnable:
    case kMethod3dCullFace:
    case kMethod3dCullFaceEnable:
    case kMethod3dIndexAddress:
    case kMethod3dIndexFormat:
    case kMethod3dBeginEnd:
        return {};

    case kMethod3dDrawArrays:
    case kMethod3dDrawElements32:
    case kMethod3dDrawElements16: {
        const u32 draw_count = val & 0x00FFFFFFu;
        const u32 draw_start = (val >> 24) & 0xFFu;
        return ctx.ValidateDraw(draw_start, draw_count, method != kMethod3dDrawArrays,
                                memory_reader);
    }

    default:
        if ((method >= kMethod3dVertexOffsetBase && method < kMethod3dVertexOffsetBase + 64) ||
            (method >= kMethod3dVertexFormatBase && method < kMethod3dVertexFormatBase + 64) ||
            (method >= kMethod3dTextureOffsetBase && method < kMethod3dTextureOffsetBase + 0x100)) {
            return {};
        }

        ctx.RecordUnsupportedMethod(class_id, method, subchannel, count, val);
        return Error{ErrorCode::UnsupportedFeature, "Unsupported pushbuffer method offset", method};
    }
}

bool Apply3dMethod(u32 method, u32 val, Nv2a3dContext& ctx, GpuSurface& back_surface,
                   PushbufferStats& stats, const PushbufferDecoder::ReadWordFn& memory_reader) {
    switch (method) {
    case kMethod3dClearSurface:
        ctx.ClearSurface(back_surface, val);
        return true;
    case kMethod3dClearColor:
        ctx.SetClearColorValue(val);
        return true;
    case kMethod3dClearDepth:
        ctx.SetClearDepthValue(U32ToFloat(val));
        return true;
    case kMethod3dSurfaceClipHorizontal:
    case kMethod3dScissorHorizontal:
        ctx.mutable_scissor().x = val & 0xFFFFu;
        ctx.mutable_scissor().width = (val >> 16) & 0xFFFFu;
        ctx.mutable_scissor().enabled = true;
        return true;
    case kMethod3dSurfaceClipVertical:
    case kMethod3dScissorVertical:
        ctx.mutable_scissor().y = val & 0xFFFFu;
        ctx.mutable_scissor().height = (val >> 16) & 0xFFFFu;
        ctx.mutable_scissor().enabled = true;
        return true;
    case kMethod3dViewportScaleX:
        ctx.mutable_viewport().scale_x = U32ToFloat(val);
        return true;
    case kMethod3dViewportScaleY:
        ctx.mutable_viewport().scale_y = U32ToFloat(val);
        return true;
    case kMethod3dViewportScaleZ:
        ctx.mutable_viewport().scale_z = U32ToFloat(val);
        return true;
    case kMethod3dViewportScaleW:
        ctx.mutable_viewport().scale_w = U32ToFloat(val);
        return true;
    case kMethod3dViewportOffsetX:
        ctx.mutable_viewport().offset_x = U32ToFloat(val);
        return true;
    case kMethod3dViewportOffsetY:
        ctx.mutable_viewport().offset_y = U32ToFloat(val);
        return true;
    case kMethod3dViewportOffsetZ:
        ctx.mutable_viewport().offset_z = U32ToFloat(val);
        return true;
    case kMethod3dViewportOffsetW:
        ctx.mutable_viewport().offset_w = U32ToFloat(val);
        return true;
    case kMethod3dBlendEnable:
        ctx.SetBlendEnabled(val != 0);
        return true;
    case kMethod3dBlendFuncSrc:
        ctx.SetBlendSrc(static_cast<BlendFactor>(val));
        return true;
    case kMethod3dBlendFuncDst:
        ctx.SetBlendDst(static_cast<BlendFactor>(val));
        return true;
    case kMethod3dBlendEquation:
        ctx.SetBlendEquation(static_cast<BlendEquation>(val));
        return true;
    case kMethod3dColorMask:
        ctx.SetColorMask(DecodeColorMask(val));
        return true;
    case kMethod3dDepthMask:
        ctx.SetDepthWriteEnabled(val != 0);
        return true;
    case kMethod3dDepthFunc:
        ctx.SetDepthFunc(static_cast<DepthFunc>(val));
        return true;
    case kMethod3dDepthTestEnable:
        ctx.SetDepthTestEnabled(val != 0);
        return true;
    case kMethod3dCullFace:
        ctx.SetCullMode(static_cast<CullMode>(val));
        return true;
    case kMethod3dCullFaceEnable:
        ctx.SetCullEnabled(val != 0);
        return true;
    case kMethod3dIndexAddress: {
        auto idx = ctx.index_buffer();
        idx.address = val;
        ctx.SetIndexBuffer(idx);
        return true;
    }
    case kMethod3dIndexFormat: {
        auto idx = ctx.index_buffer();
        idx.format = (val == 0) ? IndexFormat::Index16 : IndexFormat::Index32;
        ctx.SetIndexBuffer(idx);
        return true;
    }
    case kMethod3dBeginEnd:
        ctx.SetPrimitiveType(static_cast<PrimitiveType>(val));
        return true;
    case kMethod3dDrawArrays:
    case kMethod3dDrawElements32:
    case kMethod3dDrawElements16: {
        const u32 count = val & 0x00FFFFFFu;
        const u32 start = (val >> 24) & 0xFFu;
        auto draw_res =
            ctx.Draw(start, count, method != kMethod3dDrawArrays, back_surface, memory_reader);
        stats.vertices_processed += count;
        if (draw_res.has_value()) {
            stats.triangles_rasterized += *draw_res;
        }
        return true;
    }
    default:
        if (method >= kMethod3dVertexOffsetBase && method < kMethod3dVertexOffsetBase + 64) {
            const u32 attr_idx = (method - kMethod3dVertexOffsetBase) / 4;
            auto attr = ctx.vertex_attribute(attr_idx);
            attr.address = val;
            attr.enabled = (val != 0);
            ctx.SetVertexAttribute(attr_idx, attr);
            return true;
        }
        if (method >= kMethod3dVertexFormatBase && method < kMethod3dVertexFormatBase + 64) {
            const u32 attr_idx = (method - kMethod3dVertexFormatBase) / 4;
            auto attr = ctx.vertex_attribute(attr_idx);
            attr.stride = (val >> 8) & 0xFFu;
            attr.format = DecodeVertexFormat((val >> 4) & 0x0Fu, val & 0x0Fu);
            ctx.SetVertexAttribute(attr_idx, attr);
            return true;
        }
        if (method >= kMethod3dTextureOffsetBase && method < kMethod3dTextureOffsetBase + 0x100) {
            ApplyTextureStageMethod(method, val, ctx);
            return true;
        }
        return false;
    }
}

} // namespace xblob::gpu::detail
