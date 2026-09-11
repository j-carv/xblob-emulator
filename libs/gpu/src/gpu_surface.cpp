#include "xblob/gpu/gpu_surface.hpp"

#include "xblob/common/safe_math.hpp"

#include <cstring>

namespace xblob::gpu {

GpuSurface::GpuSurface(u32 width, u32 height, u32 pitch, u32 byte_size)
    : width_(width), height_(height), pitch_(pitch), byte_size_(byte_size), pixels_(byte_size, 0) {}

Result<GpuSurface> GpuSurface::Create(u32 width, u32 height, u32 pitch) {
    if (width == 0 || height == 0) {
        return Error{ErrorCode::InvalidArgument, "Surface dimensions must be non-zero"};
    }
    if (width > kMaxSurfaceWidth || height > kMaxSurfaceHeight) {
        return Error{ErrorCode::InvalidArgument,
                     "Surface dimensions exceed allowed hardware maximums"};
    }

    if (pitch == 0) {
        u32 default_pitch = 0;
        if (!CheckedMul(width, 4u, default_pitch)) {
            return Error{ErrorCode::IntegerOverflow, "Integer overflow computing surface pitch"};
        }
        pitch = default_pitch;
    }

    if (pitch < width * 4) {
        return Error{ErrorCode::InvalidArgument, "Pitch is smaller than width * 4 (RGBA8)"};
    }
    if (pitch > kMaxSurfacePitch) {
        return Error{ErrorCode::InvalidArgument, "Pitch exceeds maximum hardware pitch limit"};
    }

    u32 total_bytes = 0;
    if (!CheckedMul(pitch, height, total_bytes)) {
        return Error{ErrorCode::IntegerOverflow, "Integer overflow computing surface byte size"};
    }
    if (total_bytes > kMaxSurfaceBytes) {
        return Error{ErrorCode::InvalidCapacity,
                     "Surface byte size exceeds conservative hardware cap"};
    }

    return GpuSurface(width, height, pitch, total_bytes);
}

void GpuSurface::Clear(u8 r, u8 g, u8 b, u8 a) noexcept {
    for (u32 y = 0; y < height_; ++y) {
        const u32 row_offset = y * pitch_;
        for (u32 x = 0; x < width_; ++x) {
            const u32 offset = row_offset + x * 4;
            pixels_[offset + 0] = r;
            pixels_[offset + 1] = g;
            pixels_[offset + 2] = b;
            pixels_[offset + 3] = a;
        }
    }
}

Result<void> GpuSurface::FillRect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b, u8 a) {
    u32 x_end = 0;
    if (!CheckedAdd(x, w, x_end) || x_end > width_) {
        return Error{ErrorCode::OutOfBounds, "Rectangle extends outside surface width bounds"};
    }
    u32 y_end = 0;
    if (!CheckedAdd(y, h, y_end) || y_end > height_) {
        return Error{ErrorCode::OutOfBounds, "Rectangle extends outside surface height bounds"};
    }

    // Atomic commit: all coordinates validated before pixel writes
    for (u32 row = y; row < y_end; ++row) {
        const u32 row_offset = row * pitch_;
        for (u32 col = x; col < x_end; ++col) {
            const u32 offset = row_offset + col * 4;
            pixels_[offset + 0] = r;
            pixels_[offset + 1] = g;
            pixels_[offset + 2] = b;
            pixels_[offset + 3] = a;
        }
    }

    return {};
}

Result<u32> GpuSurface::GetPixel(u32 x, u32 y) const noexcept {
    if (x >= width_ || y >= height_) {
        return Error{ErrorCode::OutOfBounds, "Pixel coordinates out of bounds"};
    }
    const u32 offset = y * pitch_ + x * 4;
    const u32 r = pixels_[offset + 0];
    const u32 g = pixels_[offset + 1];
    const u32 b = pixels_[offset + 2];
    const u32 a = pixels_[offset + 3];
    return r | (g << 8) | (b << 16) | (a << 24);
}

Result<std::size_t> GpuSurface::CopyRawPixels(std::span<u8> destination) const noexcept {
    if (destination.size() < byte_size_) {
        return Error{ErrorCode::InvalidCapacity,
                     "Destination buffer smaller than frame surface size"};
    }
    std::memcpy(destination.data(), pixels_.data(), byte_size_);
    return byte_size_;
}

GpuFrameMetadata GpuSurface::metadata(u64 current_cycle) const noexcept {
    return GpuFrameMetadata{
        .width = width_,
        .height = height_,
        .pitch = pitch_,
        .pixel_format = PixelFormat::Rgba8,
        .sequence_number = sequence_number_,
        .frame_cycle = current_cycle,
        .buffer_size = byte_size_,
    };
}

} // namespace xblob::gpu
