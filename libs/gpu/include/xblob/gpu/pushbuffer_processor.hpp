#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/nv2a_3d_context.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/pushbuffer_types.hpp"

#include <array>
#include <span>
#include <unordered_set>

namespace xblob::gpu {

class PushbufferProcessor {
public:
    explicit PushbufferProcessor(Nv2aDevice& device);

    Result<void> ExecutePacket(const PushbufferPacket& packet);
    Result<PushbufferStats> ExecuteBuffer(std::span<const u32> words,
                                          PushbufferBudgets budgets = PushbufferBudgets{});
    Result<PushbufferStats> ExecuteFromMemory(GuestAddr start_addr,
                                              PushbufferDecoder::ReadWordFn reader, u32 max_words,
                                              PushbufferBudgets budgets = PushbufferBudgets{});

    [[nodiscard]] const PushbufferStats& stats() const noexcept { return stats_; }
    void ResetStats() noexcept { stats_ = {}; }

    void SetMemoryReader(PushbufferDecoder::ReadWordFn reader) noexcept {
        memory_reader_ = std::move(reader);
    }
    [[nodiscard]] const PushbufferDecoder::ReadWordFn& memory_reader() const noexcept {
        return memory_reader_;
    }

    [[nodiscard]] const Nv2a3dContext& ctx_3d() const noexcept { return ctx_3d_; }
    [[nodiscard]] Nv2a3dContext& ctx_3d() noexcept { return ctx_3d_; }

    [[nodiscard]] u32 GetSubchannelClass(u32 subchannel) const noexcept {
        if (subchannel >= kMaxSubchannels) {
            return 0;
        }
        return subchannel_classes_[subchannel];
    }

private:
    Result<void> ValidatePacket(const PushbufferPacket& packet);
    Result<void> ApplyPacket(const PushbufferPacket& packet);

    Nv2aDevice& device_;
    PushbufferStats stats_;
    Nv2a3dContext ctx_3d_;
    std::array<u32, kMaxSubchannels> subchannel_classes_{};
    PushbufferDecoder::ReadWordFn memory_reader_;

    // Staged drawing state (2D synthetic legacy)
    u32 rect_x_{0};
    u32 rect_y_{0};
    u32 rect_w_{0};
    u32 rect_h_{0};
    u32 rect_color_{0xFFFFFFFFu};
    u32 clear_color_{0x000000FFu};
    u32 surface_w_{640};
    u32 surface_h_{480};
    u32 surface_pitch_{640 * 4};
};

} // namespace xblob::gpu
