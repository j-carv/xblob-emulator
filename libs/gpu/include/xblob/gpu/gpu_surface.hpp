#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_types.hpp"

#include <span>
#include <vector>

namespace xblob::gpu {

class GpuSurface {
public:
    GpuSurface() = default;

    static Result<GpuSurface> Create(u32 width, u32 height, u32 pitch = 0);

    [[nodiscard]] u32 width() const noexcept { return width_; }
    [[nodiscard]] u32 height() const noexcept { return height_; }
    [[nodiscard]] u32 pitch() const noexcept { return pitch_; }
    [[nodiscard]] u32 byte_size() const noexcept { return byte_size_; }
    [[nodiscard]] u64 sequence_number() const noexcept { return sequence_number_; }
    [[nodiscard]] const std::vector<u8>& data() const noexcept { return pixels_; }

    void Clear(u8 r, u8 g, u8 b, u8 a) noexcept;
    Result<void> FillRect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b, u8 a);

    [[nodiscard]] Result<u32> GetPixel(u32 x, u32 y) const noexcept;
    [[nodiscard]] Result<std::size_t> CopyRawPixels(std::span<u8> destination) const noexcept;

    void SetSequenceNumber(u64 seq) noexcept { sequence_number_ = seq; }
    void IncrementSequence() noexcept { ++sequence_number_; }

    [[nodiscard]] GpuFrameMetadata metadata(u64 current_cycle = 0) const noexcept;

private:
    GpuSurface(u32 width, u32 height, u32 pitch, u32 byte_size);

    u32 width_{0};
    u32 height_{0};
    u32 pitch_{0};
    u32 byte_size_{0};
    u64 sequence_number_{0};
    std::vector<u8> pixels_;
};

} // namespace xblob::gpu
