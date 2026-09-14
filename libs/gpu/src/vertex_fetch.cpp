#include "xblob/gpu/vertex_fetch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace xblob::gpu {

namespace {

Result<void> ReadBytes(const PushbufferDecoder::ReadWordFn& reader, GuestAddr addr, u8* dest,
                       std::size_t size) {
    if (!reader) {
        return Error{ErrorCode::InvalidState, "No memory reader configured for vertex fetch"};
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

Result<float> ReadFloat(const PushbufferDecoder::ReadWordFn& reader, GuestAddr addr) {
    u8 bytes[4];
    auto res = ReadBytes(reader, addr, bytes, 4);
    if (!res.has_value()) {
        return res.error();
    }
    float f = 0.0f;
    std::memcpy(&f, bytes, 4);
    return f;
}

Result<Vertex> FetchVertex(std::span<const VertexAttribute, 16> attributes, u32 vertex_index,
                           const PushbufferDecoder::ReadWordFn& reader) {
    Vertex v;

    // Attribute 0: Position
    const auto& pos_attr = attributes[0];
    if (!pos_attr.enabled || pos_attr.format == VertexFormat::None) {
        return Error{ErrorCode::InvalidState, "Vertex position attribute 0 is not enabled"};
    }

    const GuestAddr pos_addr =
        pos_attr.address + static_cast<GuestAddr>(vertex_index * pos_attr.stride);
    switch (pos_attr.format) {
    case VertexFormat::Float2: {
        auto x_res = ReadFloat(reader, pos_addr);
        auto y_res = ReadFloat(reader, pos_addr + 4);
        if (!x_res.has_value() || !y_res.has_value()) {
            return Error{ErrorCode::OutOfBounds, "Failed to read Float2 position"};
        }
        v.x = *x_res;
        v.y = *y_res;
        v.z = 0.0f;
        v.w = 1.0f;
        break;
    }
    case VertexFormat::Float3: {
        auto x_res = ReadFloat(reader, pos_addr);
        auto y_res = ReadFloat(reader, pos_addr + 4);
        auto z_res = ReadFloat(reader, pos_addr + 8);
        if (!x_res.has_value() || !y_res.has_value() || !z_res.has_value()) {
            return Error{ErrorCode::OutOfBounds, "Failed to read Float3 position"};
        }
        v.x = *x_res;
        v.y = *y_res;
        v.z = *z_res;
        v.w = 1.0f;
        break;
    }
    case VertexFormat::Float4: {
        auto x_res = ReadFloat(reader, pos_addr);
        auto y_res = ReadFloat(reader, pos_addr + 4);
        auto z_res = ReadFloat(reader, pos_addr + 8);
        auto w_res = ReadFloat(reader, pos_addr + 12);
        if (!x_res.has_value() || !y_res.has_value() || !z_res.has_value() || !w_res.has_value()) {
            return Error{ErrorCode::OutOfBounds, "Failed to read Float4 position"};
        }
        v.x = *x_res;
        v.y = *y_res;
        v.z = *z_res;
        v.w = *w_res;
        break;
    }
    default:
        return Error{ErrorCode::UnsupportedFeature, "Unsupported position format",
                     static_cast<u64>(pos_attr.format)};
    }

    if (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z) || std::isnan(v.w) ||
        std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z) || std::isinf(v.w)) {
        return Error{ErrorCode::InvalidArgument, "Vertex coordinate contains NaN or Inf"};
    }

    // Attribute 3: Diffuse / Color
    const auto& col_attr = attributes[3].enabled ? attributes[3] : attributes[1];
    if (col_attr.enabled && col_attr.format != VertexFormat::None) {
        const GuestAddr col_addr =
            col_attr.address + static_cast<GuestAddr>(vertex_index * col_attr.stride);
        if (col_attr.format == VertexFormat::Ubyte4) {
            u8 b[4];
            auto res = ReadBytes(reader, col_addr, b, 4);
            if (!res.has_value()) {
                return Error{ErrorCode::OutOfBounds, "Failed to read Ubyte4 color"};
            }
            v.color = ColorRgba{b[0], b[1], b[2], b[3]};
        } else if (col_attr.format == VertexFormat::Float4) {
            auto r_res = ReadFloat(reader, col_addr);
            auto g_res = ReadFloat(reader, col_addr + 4);
            auto b_res = ReadFloat(reader, col_addr + 8);
            auto a_res = ReadFloat(reader, col_addr + 12);
            if (r_res.has_value() && g_res.has_value() && b_res.has_value() && a_res.has_value()) {
                v.color = ColorRgba{
                    static_cast<u8>(std::clamp(*r_res, 0.0f, 1.0f) * 255.0f),
                    static_cast<u8>(std::clamp(*g_res, 0.0f, 1.0f) * 255.0f),
                    static_cast<u8>(std::clamp(*b_res, 0.0f, 1.0f) * 255.0f),
                    static_cast<u8>(std::clamp(*a_res, 0.0f, 1.0f) * 255.0f),
                };
            }
        }
    }

    // Attribute 8: Texture Coordinates (Tex0)
    const auto& tex_attr = attributes[8].enabled ? attributes[8] : attributes[9];
    if (tex_attr.enabled && tex_attr.format != VertexFormat::None) {
        const GuestAddr tex_addr =
            tex_attr.address + static_cast<GuestAddr>(vertex_index * tex_attr.stride);
        if (tex_attr.format == VertexFormat::Float2) {
            auto u_res = ReadFloat(reader, tex_addr);
            auto v_res = ReadFloat(reader, tex_addr + 4);
            if (u_res.has_value() && v_res.has_value()) {
                v.u = *u_res;
                v.v = *v_res;
            }
        }
    }

    return v;
}

TransformedVertex Transform(const Vertex& v, const Viewport& vp) {
    TransformedVertex tv;
    tv.color = v.color;
    tv.u = v.u;
    tv.v = v.v;

    const float w = (v.w != 0.0f) ? v.w : 1.0f;
    tv.inv_w = 1.0f / w;

    if (vp.scale_x == 0.0f && vp.scale_y == 0.0f) {
        tv.screen_x = v.x;
        tv.screen_y = v.y;
        tv.screen_z = v.z;
    } else {
        tv.screen_x = vp.scale_x * (v.x * tv.inv_w) + vp.offset_x;
        tv.screen_y = vp.scale_y * (v.y * tv.inv_w) + vp.offset_y;
        tv.screen_z = vp.scale_z * (v.z * tv.inv_w) + vp.offset_z;
    }

    return tv;
}

Result<u32> ReadIndex(const IndexBufferBinding& binding, u32 index_offset,
                      const PushbufferDecoder::ReadWordFn& reader) {
    if (binding.format == IndexFormat::Index16) {
        const GuestAddr addr = binding.address + static_cast<GuestAddr>(index_offset * 2);
        u8 b[2];
        auto res = ReadBytes(reader, addr, b, 2);
        if (!res.has_value()) {
            return res.error();
        }
        return static_cast<u32>(b[0] | (static_cast<u32>(b[1]) << 8));
    } else if (binding.format == IndexFormat::Index32) {
        const GuestAddr addr = binding.address + static_cast<GuestAddr>(index_offset * 4);
        u8 b[4];
        auto res = ReadBytes(reader, addr, b, 4);
        if (!res.has_value()) {
            return res.error();
        }
        return static_cast<u32>(b[0] | (static_cast<u32>(b[1]) << 8) |
                                (static_cast<u32>(b[2]) << 16) | (static_cast<u32>(b[3]) << 24));
    }
    return Error{ErrorCode::InvalidField, "Unsupported index format"};
}

} // namespace

Result<void> VertexFetcher::ValidateFetch(std::span<const VertexAttribute, 16> attributes,
                                          const IndexBufferBinding& index_binding, u32 start,
                                          u32 count, bool indexed, u32 max_allowed_vertices,
                                          const PushbufferDecoder::ReadWordFn& reader) {
    if (count == 0) {
        return Error{ErrorCode::InvalidArgument, "Draw vertex count must be greater than zero"};
    }
    if (count > max_allowed_vertices) {
        return Error{ErrorCode::LimitReached, "Draw vertex count exceeds budget"};
    }

    const auto& pos_attr = attributes[0];
    if (!pos_attr.enabled || pos_attr.format == VertexFormat::None) {
        return Error{ErrorCode::InvalidState, "Position attribute 0 is not enabled"};
    }
    const u32 pos_bytes = GetVertexFormatBytes(pos_attr.format);
    if (pos_bytes == 0) {
        return Error{ErrorCode::UnsupportedFeature, "Unsupported position format"};
    }
    if (pos_attr.stride < pos_bytes) {
        return Error{ErrorCode::InvalidArgument, "Vertex stride smaller than format size"};
    }

    if (indexed) {
        if (index_binding.format != IndexFormat::Index16 &&
            index_binding.format != IndexFormat::Index32) {
            return Error{ErrorCode::InvalidField, "Invalid or unspecified index buffer format"};
        }
        const u32 index_elem_size = (index_binding.format == IndexFormat::Index16) ? 2 : 4;
        const u64 index_span_bytes = static_cast<u64>(start + count) * index_elem_size;
        if (static_cast<u64>(index_binding.address) + index_span_bytes > 0x100000000ULL) {
            return Error{ErrorCode::OutOfBounds, "Index buffer read exceeds guest address space"};
        }

        // Test reading all indices and checking vertex memory bounds
        for (u32 i = 0; i < count; ++i) {
            auto idx_res = ReadIndex(index_binding, start + i, reader);
            if (!idx_res.has_value()) {
                return idx_res.error();
            }
            const u32 v_idx = *idx_res;
            for (u32 a = 0; a < 16; ++a) {
                const auto& attr = attributes[a];
                if (!attr.enabled || attr.format == VertexFormat::None) {
                    continue;
                }
                const u32 attr_bytes = GetVertexFormatBytes(attr.format);
                const u64 required_offset = static_cast<u64>(v_idx) * attr.stride + attr_bytes;
                if (static_cast<u64>(attr.address) + required_offset > 0x100000000ULL) {
                    return Error{ErrorCode::OutOfBounds,
                                 "Vertex attribute fetch exceeds guest memory"};
                }
            }
        }
    } else {
        for (u32 a = 0; a < 16; ++a) {
            const auto& attr = attributes[a];
            if (!attr.enabled || attr.format == VertexFormat::None) {
                continue;
            }
            const u32 attr_bytes = GetVertexFormatBytes(attr.format);
            const u64 required_offset =
                static_cast<u64>(start + count - 1) * attr.stride + attr_bytes;
            if (static_cast<u64>(attr.address) + required_offset > 0x100000000ULL) {
                return Error{ErrorCode::OutOfBounds, "Vertex attribute fetch exceeds guest memory"};
            }
        }
    }

    return {};
}

Result<void> VertexFetcher::FetchAndAssemble(PrimitiveType prim_type,
                                             std::span<const VertexAttribute, 16> attributes,
                                             const IndexBufferBinding& index_binding, u32 start,
                                             u32 count, bool indexed, const Viewport& viewport,
                                             const PushbufferDecoder::ReadWordFn& reader,
                                             std::vector<Triangle>& out_triangles) {
    if (prim_type != PrimitiveType::Triangles && prim_type != PrimitiveType::TriangleStrip &&
        prim_type != PrimitiveType::TriangleFan) {
        return Error{ErrorCode::UnsupportedFeature, "Unsupported primitive type for 3D assembly",
                     static_cast<u64>(prim_type)};
    }

    if (count < 3) {
        return {};
    }

    std::vector<TransformedVertex> vertices;
    vertices.reserve(count);

    for (u32 i = 0; i < count; ++i) {
        u32 v_idx = start + i;
        if (indexed) {
            auto idx_res = ReadIndex(index_binding, start + i, reader);
            if (!idx_res.has_value()) {
                return idx_res.error();
            }
            v_idx = *idx_res;
        }

        auto vert_res = FetchVertex(attributes, v_idx, reader);
        if (!vert_res.has_value()) {
            return vert_res.error();
        }

        vertices.push_back(Transform(*vert_res, viewport));
    }

    if (prim_type == PrimitiveType::Triangles) {
        const u32 tri_count = count / 3;
        out_triangles.reserve(out_triangles.size() + tri_count);
        for (u32 i = 0; i < tri_count; ++i) {
            out_triangles.push_back(
                Triangle{vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]});
        }
    } else if (prim_type == PrimitiveType::TriangleStrip) {
        const u32 tri_count = count - 2;
        out_triangles.reserve(out_triangles.size() + tri_count);
        for (u32 i = 0; i < tri_count; ++i) {
            if ((i & 1) == 0) {
                out_triangles.push_back(Triangle{vertices[i], vertices[i + 1], vertices[i + 2]});
            } else {
                out_triangles.push_back(Triangle{vertices[i + 1], vertices[i], vertices[i + 2]});
            }
        }
    } else if (prim_type == PrimitiveType::TriangleFan) {
        const u32 tri_count = count - 2;
        out_triangles.reserve(out_triangles.size() + tri_count);
        for (u32 i = 1; i + 1 < count; ++i) {
            out_triangles.push_back(Triangle{vertices[0], vertices[i], vertices[i + 1]});
        }
    }

    return {};
}

} // namespace xblob::gpu
