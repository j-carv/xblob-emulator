#pragma once

#include "xblob/bus/bus.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/core/scheduler.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/gpu/nv2a_device.hpp"
#include "xblob/gpu/pushbuffer_processor.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/kernel/kernel_hle.hpp"
#include "xblob/loader/xbe_loader.hpp"
#include "xblob/machine/gpu_interrupt_source.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/pci/pci_bus_bridge.hpp"
#include "xblob/pci/pci_registry.hpp"
#include "xblob/vfs/vfs.hpp"

#include <memory>
#include <optional>
#include <span>

namespace xblob::machine {

enum class MachineState : u8 {
    Created = 0,
    Prepared = 1,
    Paused = 2,
    Faulted = 3,
    Stopped = 4,
};

[[nodiscard]] constexpr const char* ToString(MachineState state) noexcept {
    switch (state) {
    case MachineState::Created:
        return "Created";
    case MachineState::Prepared:
        return "Prepared";
    case MachineState::Paused:
        return "Paused";
    case MachineState::Faulted:
        return "Faulted";
    case MachineState::Stopped:
        return "Stopped";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, MachineState state) {
    return os << ToString(state);
}

struct MachineStepResult {
    MachineState state{MachineState::Created};
    u64 instructions_executed{0};
    Cycle cycles_consumed{0};
    cpu::StepResult cpu_result{cpu::StepResult::Ok};
};

class MachineSession {
public:
    static Result<std::unique_ptr<MachineSession>>
    Create(std::size_t ram_size = memory::kRamSizeRetail);

    ~MachineSession() = default;
    MachineSession(const MachineSession&) = delete;
    MachineSession& operator=(const MachineSession&) = delete;
    MachineSession(MachineSession&&) = delete;
    MachineSession& operator=(MachineSession&&) = delete;

    [[nodiscard]] MachineState state() const noexcept { return state_; }
    [[nodiscard]] const cpu::Cpu& cpu() const noexcept { return cpu_; }
    [[nodiscard]] cpu::Cpu& cpu() noexcept { return cpu_; }
    [[nodiscard]] const core::DeterministicScheduler& scheduler() const noexcept {
        return scheduler_;
    }
    [[nodiscard]] core::DeterministicScheduler& scheduler() noexcept { return scheduler_; }
    [[nodiscard]] const memory::AddressSpace& address_space() const noexcept { return space_; }
    [[nodiscard]] memory::AddressSpace& address_space() noexcept { return space_; }
    [[nodiscard]] const bus::Bus& bus() const noexcept { return bus_; }
    [[nodiscard]] bus::Bus& bus() noexcept { return bus_; }
    [[nodiscard]] const pci::PciRegistry& pci_registry() const noexcept { return pci_registry_; }
    [[nodiscard]] pci::PciRegistry& pci_registry() noexcept { return pci_registry_; }
    [[nodiscard]] const pci::PciBusBridge& pci_bridge() const noexcept { return pci_bridge_; }
    [[nodiscard]] pci::PciBusBridge& pci_bridge() noexcept { return pci_bridge_; }
    [[nodiscard]] const gpu::Nv2aDevice& gpu() const noexcept { return *gpu_device_; }
    [[nodiscard]] gpu::Nv2aDevice& gpu() noexcept { return *gpu_device_; }
    [[nodiscard]] std::shared_ptr<gpu::Nv2aDevice> gpu_device() const noexcept {
        return gpu_device_;
    }
    [[nodiscard]] gpu::PushbufferProcessor& pushbuffer_processor() noexcept {
        return *pushbuffer_processor_;
    }

    [[nodiscard]] const std::optional<loader::XbeLoadPlan>& load_plan() const noexcept {
        return load_plan_;
    }
    [[nodiscard]] const std::optional<loader::InitialContext>& initial_context() const noexcept {
        return initial_context_;
    }
    [[nodiscard]] const std::optional<Error>& last_error() const noexcept { return last_error_; }

    Result<void> Prepare(const ByteSource& xbe_source);
    Result<void> PrepareMedia(std::shared_ptr<const ByteSource> media_source);
    Result<MachineStepResult> Step(u64 instruction_budget = 1);
    Result<MachineStepResult> RunWithBudget(u64 max_instructions, u64 max_cycles);
    Result<void> Pause();
    Result<void> Stop();

    [[nodiscard]] const std::shared_ptr<Vfs>& vfs() const noexcept { return vfs_; }
    [[nodiscard]] const std::shared_ptr<const ByteSource>& media_source() const noexcept {
        return media_source_;
    }
    [[nodiscard]] const kernel::KernelHle& kernel() const noexcept { return kernel_; }
    [[nodiscard]] kernel::KernelHle& kernel() noexcept { return kernel_; }

    // Pushbuffer and Frame presentation support
    Result<gpu::PushbufferStats> ExecutePushbuffer(GuestAddr addr, u32 word_count,
                                                   gpu::PushbufferBudgets budgets = {});
    Result<EventId> SchedulePushbuffer(GuestAddr addr, u32 word_count, Cycle delay = 50);

    [[nodiscard]] gpu::GpuFrameMetadata GetLatestFrameMetadata() const noexcept;
    Result<std::size_t> CopyLatestFrame(std::span<u8> destination) const;

private:
    explicit MachineSession(memory::Ram ram);

    MachineState state_{MachineState::Created};
    memory::Ram ram_;
    memory::AddressSpace space_;
    bus::Bus bus_;
    core::DeterministicScheduler scheduler_;
    cpu::Cpu cpu_;
    kernel::KernelHle kernel_;
    pci::PciRegistry pci_registry_;
    pci::PciBusBridge pci_bridge_;
    std::shared_ptr<gpu::Nv2aDevice> gpu_device_;
    std::unique_ptr<gpu::PushbufferProcessor> pushbuffer_processor_;
    GpuInterruptSource gpu_interrupt_source_;

    std::shared_ptr<Vfs> vfs_;
    std::shared_ptr<const ByteSource> media_source_;

    std::optional<loader::XbeLoadPlan> load_plan_;
    std::optional<loader::InitialContext> initial_context_;
    std::optional<Error> last_error_;
};

} // namespace xblob::machine
