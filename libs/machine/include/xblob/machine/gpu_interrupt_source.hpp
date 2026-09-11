#pragma once

#include "xblob/common/types.hpp"
#include "xblob/cpu/interrupt_controller.hpp"
#include "xblob/gpu/nv2a_device.hpp"

#include <memory>
#include <optional>

namespace xblob::machine {

class GpuInterruptSource : public cpu::IInterruptSource {
public:
    explicit GpuInterruptSource(std::shared_ptr<gpu::Nv2aDevice> gpu = nullptr, u8 vector = 0x23)
        : gpu_(std::move(gpu)), vector_(vector) {}

    void SetGpu(std::shared_ptr<gpu::Nv2aDevice> gpu) noexcept { gpu_ = std::move(gpu); }
    void SetVector(u8 vector) noexcept { vector_ = vector; }
    [[nodiscard]] u8 vector() const noexcept { return vector_; }

    [[nodiscard]] bool HasPendingInterrupt() const noexcept override {
        return gpu_ != nullptr && gpu_->IsInterruptAsserted();
    }

    [[nodiscard]] std::optional<u8> AcknowledgeInterrupt() noexcept override {
        if (!HasPendingInterrupt()) {
            return std::nullopt;
        }
        return vector_;
    }

private:
    std::shared_ptr<gpu::Nv2aDevice> gpu_{nullptr};
    u8 vector_{0x23};
};

} // namespace xblob::machine
