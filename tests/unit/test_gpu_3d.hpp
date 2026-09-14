#pragma once

#include "tests/test_framework.hpp"
#include "xblob/gpu/gpu_types.hpp"
#include "xblob/gpu/nv2a_3d_types.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/pushbuffer_processor.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace xblob::gpu::testing_3d {

inline u32 FloatToU32(float f) noexcept {
    u32 u = 0;
    std::memcpy(&u, &f, sizeof(float));
    return u;
}

class MockMemory {
public:
    void Write32(GuestAddr addr, u32 val) { mem_[addr & ~0x03u] = val; }

    void WriteBytes(GuestAddr addr, const void* data, std::size_t size) {
        const u8* bytes = static_cast<const u8*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            GuestAddr byte_addr = addr + static_cast<GuestAddr>(i);
            GuestAddr word_addr = byte_addr & ~0x03u;
            u32 current = mem_[word_addr];
            u32 shift = (byte_addr & 0x03u) * 8;
            current &= ~(0xFFu << shift);
            current |= static_cast<u32>(bytes[i]) << shift;
            mem_[word_addr] = current;
        }
    }

    PushbufferDecoder::ReadWordFn GetReader() {
        return [this](GuestAddr addr) -> Result<u32> {
            auto it = mem_.find(addr & ~0x03u);
            if (it != mem_.end()) {
                return it->second;
            }
            return 0u;
        };
    }

private:
    std::map<GuestAddr, u32> mem_;
};

TEST_CASE(Test3DMethodAllowlistAndClassBinding) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);

    // 1. Valid class binding: subchannel 0 -> 0x0097 (Kelvin 3D)
    const std::vector<u32> bind_kelvin = {
        (1u << 18) | (0u << 13) | kMethodSetObject,
        kClassNv097Kelvin3D,
    };
    EXPECT_TRUE(proc.ExecuteBuffer(bind_kelvin).has_value());
    EXPECT_EQ(proc.GetSubchannelClass(0), kClassNv097Kelvin3D);

    // 2. Unsupported class ID
    const std::vector<u32> bad_class = {
        (1u << 18) | (1u << 13) | kMethodSetObject,
        0xDEADBEEFu,
    };
    auto bad_class_res = proc.ExecuteBuffer(bad_class);
    EXPECT_FALSE(bad_class_res.has_value());
    EXPECT_EQ(dev->fault(), GpuFault::UnknownMethod);
    EXPECT_TRUE(proc.ctx_3d().total_unsupported_methods_count() >= 1u);

    // 3. Unsupported method offset with bounded diagnostics
    proc.ctx_3d().ClearUnsupportedMethods();
    const std::vector<u32> bad_method = {
        (1u << 18) | (0u << 13) | 0x1E00u, // Unknown method
        0x12345678u,
    };
    auto bad_method_res = proc.ExecuteBuffer(bad_method);
    EXPECT_FALSE(bad_method_res.has_value());
    EXPECT_EQ(proc.ctx_3d().unsupported_methods().size(), 1u);
    EXPECT_EQ(proc.ctx_3d().unsupported_methods()[0].method, 0x1E00u);
}

TEST_CASE(Test3DVertexFetchAndValidation) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // Setup 3 vertices at 0x1000: Float3 position + Ubyte4 color
    struct Vtx {
        float x, y, z;
        u8 r, g, b, a;
    };
    const Vtx vertices[3] = {
        {50.0f, 20.0f, 0.5f, 255, 0, 0, 255},
        {80.0f, 80.0f, 0.5f, 0, 255, 0, 255},
        {20.0f, 80.0f, 0.5f, 0, 0, 255, 255},
    };
    mem.WriteBytes(0x1000, vertices, sizeof(vertices));

    // Configure Vertex Attribute 0 (Position: Float3, stride 16)
    VertexAttribute pos_attr;
    pos_attr.address = 0x1000;
    pos_attr.stride = sizeof(Vtx);
    pos_attr.format = VertexFormat::Float3;
    pos_attr.enabled = true;
    proc.ctx_3d().SetVertexAttribute(0, pos_attr);

    // Configure Vertex Attribute 3 (Color: Ubyte4, stride 16)
    VertexAttribute col_attr;
    col_attr.address = 0x1000 + 12;
    col_attr.stride = sizeof(Vtx);
    col_attr.format = VertexFormat::Ubyte4;
    col_attr.enabled = true;
    proc.ctx_3d().SetVertexAttribute(3, col_attr);

    // Draw 3 vertices
    auto val_res = proc.ctx_3d().ValidateDraw(0, 3, false, mem.GetReader());
    EXPECT_TRUE(val_res.has_value());

    // Invalid vertex count for TriangleList (not multiple of 3)
    auto invalid_count_res = proc.ctx_3d().ValidateDraw(0, 4, false, mem.GetReader());
    EXPECT_FALSE(invalid_count_res.has_value());

    // Zero vertices
    EXPECT_FALSE(proc.ctx_3d().ValidateDraw(0, 0, false, mem.GetReader()).has_value());

    // NaN coordinate validation
    const float nan_val = std::nanf("");
    mem.WriteBytes(0x1000, &nan_val, sizeof(float));
    auto draw_nan_res = proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader());
    EXPECT_FALSE(draw_nan_res.has_value());
}

TEST_CASE(Test3DRasterizationDeterministicAndGoldenPixels) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // Create two triangles forming a rectangle from (10, 10) to (50, 50)
    // Quad vertices:
    // V0: (10, 10), V1: (50, 10), V2: (50, 50), V3: (10, 50)
    // Tri1: V0, V1, V2; Tri2: V0, V2, V3
    struct SimpleVtx {
        float x, y;
        u8 r, g, b, a;
    };
    const SimpleVtx quad_verts[6] = {
        {10.0f, 10.0f, 255, 0, 0, 255}, {50.0f, 10.0f, 255, 0, 0, 255},
        {50.0f, 50.0f, 255, 0, 0, 255},

        {10.0f, 10.0f, 255, 0, 0, 255}, {50.0f, 50.0f, 255, 0, 0, 255},
        {10.0f, 50.0f, 255, 0, 0, 255},
    };
    mem.WriteBytes(0x2000, quad_verts, sizeof(quad_verts));

    VertexAttribute pos_attr;
    pos_attr.address = 0x2000;
    pos_attr.stride = sizeof(SimpleVtx);
    pos_attr.format = VertexFormat::Float2;
    pos_attr.enabled = true;
    proc.ctx_3d().SetVertexAttribute(0, pos_attr);

    VertexAttribute col_attr;
    col_attr.address = 0x2000 + 8;
    col_attr.stride = sizeof(SimpleVtx);
    col_attr.format = VertexFormat::Ubyte4;
    col_attr.enabled = true;
    proc.ctx_3d().SetVertexAttribute(3, col_attr);

    // Viewport scale 0 = direct screen coordinates
    proc.ctx_3d().mutable_viewport().scale_x = 0.0f;
    proc.ctx_3d().mutable_viewport().scale_y = 0.0f;

    // Clear surface to black
    proc.ctx_3d().ClearColor(dev->back_surface(), 0, 0, 0, 255);

    // Draw quad (6 vertices)
    auto draw_res = proc.ctx_3d().Draw(0, 6, false, dev->back_surface(), mem.GetReader());
    EXPECT_TRUE(draw_res.has_value());
    EXPECT_EQ(*draw_res, 2u); // 2 triangles

    // Verify interior pixels are red (0xFF0000FF in little-endian byte order: r=255, g=0, b=0,
    // a=255)
    EXPECT_EQ(*dev->back_surface().GetPixel(25, 25), 0xFF0000FFu);
    EXPECT_EQ(*dev->back_surface().GetPixel(30, 30), 0xFF0000FFu);

    // Verify shared diagonal boundary (x=30, y=30) is covered seamlessly
    EXPECT_EQ(*dev->back_surface().GetPixel(30, 30), 0xFF0000FFu);

    // Verify outside pixels remain black
    EXPECT_EQ(*dev->back_surface().GetPixel(5, 5), 0xFF000000u);
    EXPECT_EQ(*dev->back_surface().GetPixel(60, 60), 0xFF000000u);
}

TEST_CASE(Test3DDepthBufferingAndScissor) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // Tri 1: Red at depth 0.5
    // Tri 2: Green at depth 0.8 (behind Tri 1 -> should fail LessEqual test)
    // Tri 3: Blue at depth 0.2 (in front -> should overwrite Tri 1)
    struct DepthVtx {
        float x, y, z;
        u8 r, g, b, a;
    };
    const DepthVtx v1[3] = {
        {10.0f, 10.0f, 0.5f, 255, 0, 0, 255},
        {50.0f, 10.0f, 0.5f, 255, 0, 0, 255},
        {10.0f, 50.0f, 0.5f, 255, 0, 0, 255},
    };
    const DepthVtx v2[3] = {
        {10.0f, 10.0f, 0.8f, 0, 255, 0, 255},
        {50.0f, 10.0f, 0.8f, 0, 255, 0, 255},
        {10.0f, 50.0f, 0.8f, 0, 255, 0, 255},
    };
    const DepthVtx v3[3] = {
        {10.0f, 10.0f, 0.2f, 0, 0, 255, 255},
        {50.0f, 10.0f, 0.2f, 0, 0, 255, 255},
        {10.0f, 50.0f, 0.2f, 0, 0, 255, 255},
    };

    mem.WriteBytes(0x3000, v1, sizeof(v1));
    mem.WriteBytes(0x3100, v2, sizeof(v2));
    mem.WriteBytes(0x3200, v3, sizeof(v3));

    proc.ctx_3d().mutable_viewport().scale_x = 0.0f;
    proc.ctx_3d().mutable_viewport().scale_y = 0.0f;
    proc.ctx_3d().SetDepthTestEnabled(true);
    proc.ctx_3d().SetDepthWriteEnabled(true);
    proc.ctx_3d().SetDepthFunc(DepthFunc::LessEqual);
    proc.ctx_3d().ClearDepth(1.0f);
    proc.ctx_3d().ClearColor(dev->back_surface(), 0, 0, 0, 255);

    VertexAttribute pos;
    pos.address = 0x3000;
    pos.stride = sizeof(DepthVtx);
    pos.format = VertexFormat::Float3;
    pos.enabled = true;
    proc.ctx_3d().SetVertexAttribute(0, pos);

    VertexAttribute col;
    col.address = 0x3000 + 12;
    col.stride = sizeof(DepthVtx);
    col.format = VertexFormat::Ubyte4;
    col.enabled = true;
    proc.ctx_3d().SetVertexAttribute(3, col);

    // Draw Tri 1 (Red, depth 0.5)
    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());
    EXPECT_EQ(*dev->back_surface().GetPixel(20, 20), 0xFF0000FFu);
    EXPECT_TRUE(std::abs(proc.ctx_3d().GetDepthPixel(20, 20) - 0.5f) < 1e-4f);

    // Draw Tri 2 (Green, depth 0.8) -> must NOT overwrite
    pos.address = 0x3100;
    col.address = 0x3100 + 12;
    proc.ctx_3d().SetVertexAttribute(0, pos);
    proc.ctx_3d().SetVertexAttribute(3, col);
    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());
    EXPECT_EQ(*dev->back_surface().GetPixel(20, 20), 0xFF0000FFu); // Still Red!

    // Draw Tri 3 (Blue, depth 0.2) -> MUST overwrite
    pos.address = 0x3200;
    col.address = 0x3200 + 12;
    proc.ctx_3d().SetVertexAttribute(0, pos);
    proc.ctx_3d().SetVertexAttribute(3, col);
    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());
    EXPECT_EQ(*dev->back_surface().GetPixel(20, 20), 0xFFFF0000u); // Blue!
    EXPECT_TRUE(std::abs(proc.ctx_3d().GetDepthPixel(20, 20) - 0.2f) < 1e-4f);

    // Scissor test
    proc.ctx_3d().ClearColor(dev->back_surface(), 0, 0, 0, 255);
    auto& sc = proc.ctx_3d().mutable_scissor();
    sc.x = 20;
    sc.y = 20;
    sc.width = 10;
    sc.height = 10;
    sc.enabled = true;

    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());
    // Inside scissor (25, 25) should be Blue
    EXPECT_EQ(*dev->back_surface().GetPixel(25, 25), 0xFFFF0000u);
    // Outside scissor (15, 15) should be Black
    EXPECT_EQ(*dev->back_surface().GetPixel(15, 15), 0xFF000000u);
}

TEST_CASE(Test3DTextureSamplingLinearAndSwizzled) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // 1. Swizzle Morton interleaving math check
    EXPECT_EQ(TextureSampler::ComputeSwizzleOffset(0, 0, 4, 4), 0u);
    EXPECT_EQ(TextureSampler::ComputeSwizzleOffset(1, 0, 4, 4), 1u);
    EXPECT_EQ(TextureSampler::ComputeSwizzleOffset(0, 1, 4, 4), 2u);
    EXPECT_EQ(TextureSampler::ComputeSwizzleOffset(1, 1, 4, 4), 3u);

    // 2. Linear 2x2 texture with 4 pixels:
    // (0,0): Red (0xFF0000FF)
    // (1,0): Green (0xFF00FF00)
    // (0,1): Blue (0xFFFF0000)
    // (1,1): White (0xFFFFFFFF)
    const u32 tex_pixels[4] = {0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFFFFu};
    mem.WriteBytes(0x4000, tex_pixels, sizeof(tex_pixels));

    TextureStage stage;
    stage.address = 0x4000;
    stage.width = 2;
    stage.height = 2;
    stage.pitch = 8;
    stage.format = TextureFormat::R8G8B8A8;
    stage.layout = TextureLayout::Linear;
    stage.enabled = true;
    proc.ctx_3d().SetTextureStage(0, stage);

    // Sample (0.25, 0.25) -> top-left Red
    auto s1 = TextureSampler::SampleNearest(stage, 0.25f, 0.25f, mem.GetReader());
    EXPECT_TRUE(s1.has_value());
    EXPECT_EQ(s1->ToU32(), 0xFF0000FFu);

    // Sample (0.75, 0.25) -> top-right Green
    auto s2 = TextureSampler::SampleNearest(stage, 0.75f, 0.25f, mem.GetReader());
    EXPECT_TRUE(s2.has_value());
    EXPECT_EQ(s2->ToU32(), 0xFF00FF00u);

    // Swizzled 2x2 texture sampling
    stage.layout = TextureLayout::Swizzled;
    // In swizzled 2x2:
    // offset 0: (0, 0), offset 1: (1, 0), offset 2: (0, 1), offset 3: (1, 1)
    auto s_swiz = TextureSampler::SampleNearest(stage, 0.25f, 0.75f, mem.GetReader());
    EXPECT_TRUE(s_swiz.has_value());
    EXPECT_EQ(s_swiz->ToU32(), 0xFFFF0000u); // Blue
}

TEST_CASE(Test3DAlphaBlendingAndColorMask) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // Base surface is Blue (0, 0, 255, 255)
    proc.ctx_3d().ClearColor(dev->back_surface(), 0, 0, 255, 255);

    // Triangle with 50% transparent Red (255, 0, 0, 128)
    struct BlendVtx {
        float x, y;
        u8 r, g, b, a;
    };
    const BlendVtx tri[3] = {
        {10.0f, 10.0f, 255, 0, 0, 128},
        {50.0f, 10.0f, 255, 0, 0, 128},
        {10.0f, 50.0f, 255, 0, 0, 128},
    };
    mem.WriteBytes(0x5000, tri, sizeof(tri));

    VertexAttribute pos;
    pos.address = 0x5000;
    pos.stride = sizeof(BlendVtx);
    pos.format = VertexFormat::Float2;
    pos.enabled = true;
    proc.ctx_3d().SetVertexAttribute(0, pos);

    VertexAttribute col;
    col.address = 0x5000 + 8;
    col.stride = sizeof(BlendVtx);
    col.format = VertexFormat::Ubyte4;
    col.enabled = true;
    proc.ctx_3d().SetVertexAttribute(3, col);

    proc.ctx_3d().mutable_viewport().scale_x = 0.0f;
    proc.ctx_3d().mutable_viewport().scale_y = 0.0f;

    // Enable Blend: SrcAlpha, InvSrcAlpha
    proc.ctx_3d().SetBlendEnabled(true);
    proc.ctx_3d().SetBlendSrc(BlendFactor::SrcAlpha);
    proc.ctx_3d().SetBlendDst(BlendFactor::InvSrcAlpha);

    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());

    // Expected pixel blend:
    // Red: 255 * (128/255) + 0 = ~128
    // Blue: 0 + 255 * (1 - 128/255) = ~127
    // Green: 0
    u32 blended = *dev->back_surface().GetPixel(20, 20);
    u8 r = blended & 0xFF;
    u8 g = (blended >> 8) & 0xFF;
    u8 b = (blended >> 16) & 0xFF;
    EXPECT_TRUE(std::abs(static_cast<int>(r) - 128) <= 2);
    EXPECT_EQ(g, 0u);
    EXPECT_TRUE(std::abs(static_cast<int>(b) - 127) <= 2);

    // Color Mask: disable Red write, draw solid Red over result
    proc.ctx_3d().SetBlendEnabled(false);
    ColorMask mask;
    mask.write_r = false; // Protect Red
    mask.write_g = true;
    mask.write_b = true;
    mask.write_a = true;
    proc.ctx_3d().SetColorMask(mask);

    EXPECT_TRUE(proc.ctx_3d().Draw(0, 3, false, dev->back_surface(), mem.GetReader()).has_value());

    u32 masked_pix = *dev->back_surface().GetPixel(20, 20);
    // Red component should have remained ~128!
    EXPECT_EQ(masked_pix & 0xFF, r);
}

TEST_CASE(Test3DPushbufferStreamAndFlip) {
    auto dev = Nv2aDevice::Create();
    PushbufferProcessor proc(*dev);
    MockMemory mem;
    proc.SetMemoryReader(mem.GetReader());

    // Triangle vertices in guest RAM at 0x6000
    struct Vtx {
        float x, y;
        u8 r, g, b, a;
    };
    const Vtx tri[3] = {
        {10.0f, 10.0f, 0, 255, 0, 255},
        {50.0f, 10.0f, 0, 255, 0, 255},
        {10.0f, 50.0f, 0, 255, 0, 255},
    };
    mem.WriteBytes(0x6000, tri, sizeof(tri));

    const std::vector<u32> stream = {
        // 1. Bind Kelvin 3D to subchannel 0
        (1u << 18) | (0u << 13) | kMethodSetObject,
        kClassNv097Kelvin3D,

        // 2. Set Clear Color & Clear Surface
        (1u << 18) | (0u << 13) | kMethod3dClearColor,
        0xFF000000u, // Black
        (1u << 18) | (0u << 13) | kMethod3dClearSurface,
        0x03u, // Clear color + depth

        // 3. Set Viewport scale 0
        (2u << 18) | (0u << 13) | kMethod3dViewportScaleX,
        0u,
        0u,

        // 4. Attribute 0: Float2 at 0x6000, stride 12
        (1u << 18) | (0u << 13) | kMethod3dVertexOffsetBase,
        0x6000u,
        (1u << 18) | (0u << 13) | kMethod3dVertexFormatBase,
        (12u << 8) | (2u << 4) | 2u, // stride 12, type Float(2), count 2

        // 5. Attribute 3: Ubyte4 at 0x6008, stride 12
        (1u << 18) | (0u << 13) | (kMethod3dVertexOffsetBase + 12),
        0x6008u,
        (1u << 18) | (0u << 13) | (kMethod3dVertexFormatBase + 12),
        (12u << 8) | (4u << 4) | 4u, // stride 12, type Ubyte4(4), count 4

        // 6. Begin Triangles
        (1u << 18) | (0u << 13) | kMethod3dBeginEnd,
        static_cast<u32>(PrimitiveType::Triangles),

        // 7. DrawArrays: count=3, start=0
        (1u << 18) | (0u << 13) | kMethod3dDrawArrays,
        3u,

        // 8. Flip
        (1u << 18) | (0u << 13) | kMethodFlip,
        1u,
    };

    auto stats_res = proc.ExecuteBuffer(stream);
    EXPECT_TRUE(stats_res.has_value());
    EXPECT_EQ(stats_res->vertices_processed, 3u);
    EXPECT_EQ(stats_res->triangles_rasterized, 1u);
    EXPECT_EQ(dev->frame_counter(), 1u);

    // Front surface should have green pixel at (20, 20)
    EXPECT_EQ(*dev->front_surface().GetPixel(20, 20), 0xFF00FF00u);
}
} // namespace xblob::gpu::testing_3d
