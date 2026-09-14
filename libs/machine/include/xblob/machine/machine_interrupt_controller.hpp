#pragma once

#include "xblob/cpu/interrupt_controller.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/usb/ohci_controller.hpp"

#include <memory>
#include <optional>

namespace xblob::machine {

class MachineInterruptController : public cpu::IInterruptSource {
public:
    explicit MachineInterruptController(std::shared_ptr<gpu::Nv2aDevice> gpu = nullptr,
                                        usb::OhciController* ohci = nullptr, u8 gpu_vector = 0x23,
                                        u8 usb_vector = 0x24)
        : gpu_(std::move(gpu)), ohci_(ohci), gpu_vector_(gpu_vector), usb_vector_(usb_vector) {}

    void SetGpu(std::shared_ptr<gpu::Nv2aDevice> gpu) noexcept { gpu_ = std::move(gpu); }
    void SetOhci(usb::OhciController* ohci) noexcept { ohci_ = ohci; }
    void SetGpuVector(u8 vector) noexcept { gpu_vector_ = vector; }
    void SetUsbVector(u8 vector) noexcept { usb_vector_ = vector; }

    [[nodiscard]] u8 gpu_vector() const noexcept { return gpu_vector_; }
    [[nodiscard]] u8 usb_vector() const noexcept { return usb_vector_; }

    [[nodiscard]] bool HasPendingInterrupt() const noexcept override {
        bool gpu_irq = (gpu_ != nullptr && gpu_->IsInterruptAsserted());
        bool usb_irq = (ohci_ != nullptr && ohci_->IsIrqAsserted());
        return gpu_irq || usb_irq;
    }

    [[nodiscard]] std::optional<u8> AcknowledgeInterrupt() noexcept override {
        if (gpu_ != nullptr && gpu_->IsInterruptAsserted()) {
            return gpu_vector_;
        }
        if (ohci_ != nullptr && ohci_->IsIrqAsserted()) {
            return usb_vector_;
        }
        return std::nullopt;
    }

private:
    std::shared_ptr<gpu::Nv2aDevice> gpu_{nullptr};
    usb::OhciController* ohci_{nullptr};
    u8 gpu_vector_{0x23};
    u8 usb_vector_{0x24};
};

} // namespace xblob::machine
