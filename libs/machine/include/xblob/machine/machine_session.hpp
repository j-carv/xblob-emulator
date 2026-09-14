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
#include "xblob/machine/execution_types.hpp"
#include "xblob/machine/gpu_interrupt_source.hpp"
#include "xblob/machine/trace_buffer.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/pci/pci_bus_bridge.hpp"
#include "xblob/pci/pci_registry.hpp"
#include "xblob/vfs/vfs.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>

namespace xblob::machine {

enum class MachineState : u8 {
    Created = 0,
    Prepared = 1,
    Paused = 2,
    Faulted = 3,
    Stopped = 4,
    Running = 5,
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
    case MachineState::Running:
        return "Running";
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

    ~MachineSession();
    MachineSession(const MachineSession&) = delete;
    MachineSession& operator=(const MachineSession&) = delete;
    MachineSession(MachineSession&&) = delete;
    MachineSession& operator=(MachineSession&&) = delete;

    [[nodiscard]] MachineState state() const noexcept;
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
    [[nodiscard]] const std::optional<Error>& last_error() const noexcept;

    Result<void> Prepare(const ByteSource& xbe_source);
    Result<void> PrepareMedia(std::shared_ptr<const ByteSource> media_source);

    // Execution control
    Result<void> Start(ExecutionBudgets budgets = {});
    Result<void> Resume(ExecutionBudgets budgets = {});
    Result<MachineStepResult> Step(u64 instruction_budget = 1);
    Result<MachineStepResult> RunWithBudget(u64 max_instructions, u64 max_cycles);
    Result<void> Pause();
    Result<void> Stop();
    Result<bool> WaitCompletion(u32 timeout_ms = 0);

    [[nodiscard]] MachineSnapshot GetSnapshot() const;
    [[nodiscard]] CompatibilityDiagnostic GetCompatibilityDiagnostic() const;
    [[nodiscard]] StopReason last_stop_reason() const;

    [[nodiscard]] TraceRingBuffer& trace_buffer() noexcept { return trace_buffer_; }
    [[nodiscard]] const TraceRingBuffer& trace_buffer() const noexcept { return trace_buffer_; }

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

    enum class CommandType { None, Start, Resume, Step, RunWithBudget, Pause, Stop };

    struct MachineCommand {
        CommandType type{CommandType::None};
        ExecutionBudgets budgets{};
        u64 step_instruction_budget{0};
        u64 run_max_cycles{0};
        std::promise<Result<MachineStepResult>>* promise{nullptr};
    };

    void WorkerLoop();
    void ExecuteCommandLocked(MachineCommand& cmd, std::unique_lock<std::mutex>& lock);
    void ExecuteRunningSliceLocked(std::unique_lock<std::mutex>& lock);
    void PopulateFaultStopReasonLocked(const cpu::StepOutcome& outcome);
    void RecordFirstBlockerLocked(const StopReason& reason);
    [[nodiscard]] MachineSnapshot TakeSnapshotLocked() const;

    mutable std::mutex mutex_;
    std::condition_variable cv_cmd_;
    std::condition_variable cv_done_;
    std::thread worker_thread_;
    std::deque<MachineCommand> command_queue_;
    bool worker_exit_{false};
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> stop_requested_{false};

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

    // Execution state and metrics
    ExecutionBudgets active_budgets_{};
    u64 total_instructions_executed_{0};
    u64 total_events_fired_{0};
    u64 session_instructions_executed_{0};
    Cycle session_cycles_consumed_{0};
    std::chrono::steady_clock::time_point session_start_time_{};

    StopReason last_stop_reason_{};
    std::optional<StopReason> first_blocker_{std::nullopt};
    u64 first_blocker_instructions_{0};
    Cycle first_blocker_cycles_{0};

    MachineSnapshot last_snapshot_{};
    TraceRingBuffer trace_buffer_{1024};
};

} // namespace xblob::machine
