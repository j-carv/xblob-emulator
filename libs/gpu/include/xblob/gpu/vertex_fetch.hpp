#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/nv2a_3d_types.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/texture_sampler.hpp"

#include <span>
#include <vector>

namespace xblob::gpu {

struct Vertex {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
    ColorRgba color{255, 255, 255, 255};
    float u{0.0f};
    float v{0.0f};
};

struct TransformedVertex {
    float screen_x{0.0f};
    float screen_y{0.0f};
    float screen_z{0.0f};
    float inv_w{1.0f};
    ColorRgba color{255, 255, 255, 255};
    float u{0.0f};
    float v{0.0f};
};

struct Triangle {
    TransformedVertex v[3];
};

class VertexFetcher {
public:
    static Result<void> ValidateFetch(std::span<const VertexAttribute, 16> attributes,
                                      const IndexBufferBinding& index_binding, u32 start, u32 count,
                                      bool indexed, u32 max_allowed_vertices,
                                      const PushbufferDecoder::ReadWordFn& reader);

    static Result<void> FetchAndAssemble(PrimitiveType prim_type,
                                         std::span<const VertexAttribute, 16> attributes,
                                         const IndexBufferBinding& index_binding, u32 start,
                                         u32 count, bool indexed, const Viewport& viewport,
                                         const PushbufferDecoder::ReadWordFn& reader,
                                         std::vector<Triangle>& out_triangles);
};

} // namespace xblob::gpu
