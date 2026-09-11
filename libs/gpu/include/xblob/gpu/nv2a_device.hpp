#pragma once

#include "xblob/bus/device.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_register_file.hpp"
#include "xblob/gpu/gpu_surface.hpp"
#include "xblob/gpu/gpu_types.hpp"
#include "xblob/pci/pci_device.hpp"

#include <memory>
#include <vector>

namespace xblob::gpu {

class Nv2aDevice : public pci::PciDevice,
                   public bus::BusDevice,
                   public std::enable_shared_from_this<Nv2aDevice> {
public:
    static std::shared_ptr<Nv2aDevice> Create(pci::PciBdf bdf = pci::PciBdf{0, 0, 0});

    explicit Nv2aDevice(pci::PciBdf bdf = pci::PciBdf{0, 0, 0});
    ~Nv2aDevice() override = default;

    // BusDevice implementation
    [[nodiscard]] std::string_view name() const noexcept override { return "NV2A GPU"; }
    [[nodiscard]] Result<u32> Read(u32 offset, bus::BusAccessWidth width) override;
    [[nodiscard]] Result<void> Write(u32 offset, bus::BusAccessWidth width, u32 value) override;

    // Lifecycle and deterministic reset
    void Reset() noexcept;

    // Registers and state
    [[nodiscard]] GpuRegisterFile& register_file() noexcept { return register_file_; }
    [[nodiscard]] const GpuRegisterFile& register_file() const noexcept { return register_file_; }

    [[nodiscard]] GpuFault fault() const noexcept { return fault_; }
    void SetFault(GpuFault f) noexcept { fault_ = f; }
    void ClearFault() noexcept { fault_ = GpuFault::None; }

    // Interrupt status
    [[nodiscard]] bool IsInterruptAsserted() const noexcept;
    void TriggerVideoInterrupt(u32 flags) noexcept;
    void AcknowledgeVideoInterrupt(u32 flags) noexcept;

    // Surface / Presentation
    [[nodiscard]] const GpuSurface& front_surface() const noexcept { return front_surface_; }
    [[nodiscard]] GpuSurface& front_surface() noexcept { return front_surface_; }
    [[nodiscard]] const GpuSurface& back_surface() const noexcept { return back_surface_; }
    [[nodiscard]] GpuSurface& back_surface() noexcept { return back_surface_; }
    [[nodiscard]] u64 frame_counter() const noexcept { return frame_counter_; }

    void Flip() noexcept;

    // Scheduled event tracking
    void RegisterPendingEvent(EventId id) { pending_event_ids_.push_back(id); }
    [[nodiscard]] const std::vector<EventId>& pending_events() const noexcept {
        return pending_event_ids_;
    }
    void ClearPendingEvents() noexcept { pending_event_ids_.clear(); }

private:
    void InitPciHeader();

    GpuRegisterFile register_file_;
    GpuSurface front_surface_;
    GpuSurface back_surface_;
    GpuFault fault_{GpuFault::None};
    u64 frame_counter_{0};
    std::vector<EventId> pending_event_ids_;
};

} // namespace xblob::gpu
