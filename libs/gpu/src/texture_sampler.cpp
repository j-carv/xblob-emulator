#include "xblob/gpu/texture_sampler.hpp"

#include <algorithm>
#include <cmath>

namespace xblob::gpu {

namespace {

Result<void> ReadBytesFromReader(const PushbufferDecoder::ReadWordFn& reader, GuestAddr addr,
                                 u8* dest, std::size_t size) {
    if (!reader) {
        return Error{ErrorCode::InvalidState, "No memory reader configured for texture sampling"};
    }
    std::size_t remaining = size;
    std::size_t offset = 0;
    while (remaining > 0) {
        GuestAddr word_addr = (addr + static_cast<GuestAddr>(offset)) & ~0x03u;
        auto word_res = reader(word_addr);
        if (!word_res.has_value()) {
            return word_res.error();
        }
        const u32 word = *word_res;
        const u32 byte_in_word = (addr + static_cast<GuestAddr>(offset)) & 0x03u;
        const u32 take =
            static_cast<u32>(std::min(static_cast<std::size_t>(4 - byte_in_word), remaining));
        for (u32 b = 0; b < take; ++b) {
            dest[offset + b] = static_cast<u8>((word >> ((byte_in_word + b) * 8)) & 0xFF);
        }
        offset += take;
        remaining -= take;
    }
    return {};
}

} // namespace

u32 TextureSampler::ComputeSwizzleOffset(u32 x, u32 y, u32 width, u32 height) noexcept {
    u32 offset = 0;
    u32 shift = 0;
    u32 mask = 1;
    while (width > 1 || height > 1) {
        if (width > 1) {
            if ((x & mask) != 0) {
                offset |= (1u << shift);
            }
            ++shift;
            width >>= 1;
        }
        if (height > 1) {
            if ((y & mask) != 0) {
                offset |= (1u << shift);
            }
            ++shift;
            height >>= 1;
        }
        mask <<= 1;
    }
    return offset;
}

Result<void> TextureSampler::ValidateStage(const TextureStage& stage) {
    if (!stage.enabled) {
        return {};
    }

    if (stage.width == 0 || stage.height == 0 || stage.width > 2048 || stage.height > 2048) {
        return Error{ErrorCode::InvalidArgument, "Invalid texture dimensions"};
    }

    const u32 bpp = GetTextureBytesPerPixel(stage.format);
    if (bpp == 0) {
        return Error{ErrorCode::UnsupportedFeature, "Unsupported texture format",
                     static_cast<u64>(stage.format)};
    }

    u64 total_bytes = 0;
    if (stage.layout == TextureLayout::Linear) {
        if (stage.pitch < stage.width * bpp) {
            return Error{ErrorCode::InvalidArgument, "Texture pitch is smaller than row bytes"};
        }
        total_bytes = static_cast<u64>(stage.height) * stage.pitch;
    } else {
        total_bytes = static_cast<u64>(stage.width) * stage.height * bpp;
    }

    if (static_cast<u64>(stage.address) + total_bytes > 0x100000000ULL) {
        return Error{ErrorCode::OutOfBounds,
                     "Texture memory range exceeds 32-bit guest address space"};
    }

    return {};
}

Result<ColorRgba> TextureSampler::SampleNearest(const TextureStage& stage, float u, float v,
                                                const PushbufferDecoder::ReadWordFn& reader) {
    if (!stage.enabled) {
        return ColorRgba{255, 255, 255, 255};
    }

    const u32 bpp = GetTextureBytesPerPixel(stage.format);
    if (bpp == 0) {
        return Error{ErrorCode::UnsupportedFeature, "Unsupported texture format",
                     static_cast<u64>(stage.format)};
    }

    // Wrap / Clamp U
    float u_norm = u;
    if (stage.wrap_u == TextureAddressMode::ClampToEdge ||
        stage.wrap_u == TextureAddressMode::ClampToBorder) {
        u_norm = std::clamp(u_norm, 0.0f, 1.0f);
    } else {
        u_norm = u_norm - std::floor(u_norm);
    }
    u32 tx = static_cast<u32>(u_norm * static_cast<float>(stage.width));
    if (tx >= stage.width) {
        tx = stage.width - 1;
    }

    // Wrap / Clamp V
    float v_norm = v;
    if (stage.wrap_v == TextureAddressMode::ClampToEdge ||
        stage.wrap_v == TextureAddressMode::ClampToBorder) {
        v_norm = std::clamp(v_norm, 0.0f, 1.0f);
    } else {
        v_norm = v_norm - std::floor(v_norm);
    }
    u32 ty = static_cast<u32>(v_norm * static_cast<float>(stage.height));
    if (ty >= stage.height) {
        ty = stage.height - 1;
    }

    // Calculate texel memory address
    GuestAddr texel_addr = 0;
    if (stage.layout == TextureLayout::Linear) {
        texel_addr = stage.address + static_cast<GuestAddr>(ty * stage.pitch + tx * bpp);
    } else {
        const u32 swizzled_idx = ComputeSwizzleOffset(tx, ty, stage.width, stage.height);
        texel_addr = stage.address + static_cast<GuestAddr>(swizzled_idx * bpp);
    }

    u8 raw[4] = {0, 0, 0, 0};
    auto read_res = ReadBytesFromReader(reader, texel_addr, raw, bpp);
    if (!read_res.has_value()) {
        return read_res.error();
    }

    switch (stage.format) {
    case TextureFormat::R8G8B8A8:
        return ColorRgba{raw[0], raw[1], raw[2], raw[3]};
    case TextureFormat::A8R8G8B8:
        // Memory order: B, G, R, A
        return ColorRgba{raw[2], raw[1], raw[0], raw[3]};
    case TextureFormat::X8R8G8B8:
        // Memory order: B, G, R, X
        return ColorRgba{raw[2], raw[1], raw[0], 255};
    case TextureFormat::R5G6B5: {
        const u16 val = static_cast<u16>(raw[0] | (static_cast<u16>(raw[1]) << 8));
        const u8 r = static_cast<u8>(((val >> 11) & 0x1F) * 255 / 31);
        const u8 g = static_cast<u8>(((val >> 5) & 0x3F) * 255 / 63);
        const u8 b = static_cast<u8>((val & 0x1F) * 255 / 31);
        return ColorRgba{r, g, b, 255};
    }
    default:
        return Error{ErrorCode::UnsupportedFeature, "Unsupported texture format",
                     static_cast<u64>(stage.format)};
    }
}

} // namespace xblob::gpu
