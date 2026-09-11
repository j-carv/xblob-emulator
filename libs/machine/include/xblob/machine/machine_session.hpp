#pragma once

#include "xblob/bus/bus.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/core/scheduler.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/loader/xbe_loader.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"

#include <memory>
#include <optional>

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
    MachineSession(MachineSession&&) = default;
    MachineSession& operator=(MachineSession&&) = default;

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
    [[nodiscard]] const std::optional<loader::XbeLoadPlan>& load_plan() const noexcept {
        return load_plan_;
    }
    [[nodiscard]] const std::optional<loader::InitialContext>& initial_context() const noexcept {
        return initial_context_;
    }
    [[nodiscard]] const std::optional<Error>& last_error() const noexcept { return last_error_; }

    Result<void> Prepare(const ByteSource& xbe_source);
    Result<MachineStepResult> Step(u64 instruction_budget = 1);
    Result<MachineStepResult> RunWithBudget(u64 max_instructions, u64 max_cycles);
    Result<void> Pause();
    Result<void> Stop();

private:
    explicit MachineSession(memory::Ram ram);

    MachineState state_{MachineState::Created};
    memory::Ram ram_;
    memory::AddressSpace space_;
    bus::Bus bus_;
    core::DeterministicScheduler scheduler_;
    cpu::Cpu cpu_;
    std::optional<loader::XbeLoadPlan> load_plan_;
    std::optional<loader::InitialContext> initial_context_;
    std::optional<Error> last_error_;
};

} // namespace xblob::machine
