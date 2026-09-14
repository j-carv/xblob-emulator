#pragma once

#include "xblob/gpu/gpu_surface.hpp"
#include "xblob/gpu/nv2a_3d_context.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/pushbuffer_types.hpp"

namespace xblob::gpu::detail {

ColorMask DecodeColorMask(u32 val) noexcept;
void ApplyTextureStageMethod(u32 method, u32 val, Nv2a3dContext& ctx) noexcept;

Result<void> Validate3dMethod(u32 method, u32 val, u32 count, u32 subchannel, u32 class_id,
                              Nv2a3dContext& ctx,
                              const PushbufferDecoder::ReadWordFn& memory_reader);

bool Apply3dMethod(u32 method, u32 val, Nv2a3dContext& ctx, GpuSurface& back_surface,
                   PushbufferStats& stats, const PushbufferDecoder::ReadWordFn& memory_reader);

} // namespace xblob::gpu::detail
