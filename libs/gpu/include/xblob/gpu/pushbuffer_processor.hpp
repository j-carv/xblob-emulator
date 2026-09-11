#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/gpu/pushbuffer_decoder.hpp"
#include "xblob/gpu/pushbuffer_types.hpp"

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

private:
    Result<void> ValidatePacket(const PushbufferPacket& packet) const;
    Result<void> ApplyPacket(const PushbufferPacket& packet);

    Nv2aDevice& device_;
    PushbufferStats stats_;

    // Staged drawing state
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
