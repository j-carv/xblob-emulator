#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/exceptions.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/cpu/instruction.hpp"
#include "xblob/cpu/instructions.hpp"
#include "xblob/cpu/interrupt_controller.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

#include <limits>
#include <optional>
#include <ostream>

namespace xblob::cpu {

enum class CpuLifecycle { Running, Halted, Faulted };

inline std::ostream& operator<<(std::ostream& os, CpuLifecycle lc) {
    switch (lc) {
    case CpuLifecycle::Running:
        return os << "Running";
    case CpuLifecycle::Halted:
        return os << "Halted";
    case CpuLifecycle::Faulted:
        return os << "Faulted";
    }
    return os << "Unknown";
}

enum class StepResult { Ok, Halted, Faulted };

inline std::ostream& operator<<(std::ostream& os, StepResult sr) {
    switch (sr) {
    case StepResult::Ok:
        return os << "Ok";
    case StepResult::Halted:
        return os << "Halted";
    case StepResult::Faulted:
        return os << "Faulted";
    }
    return os << "Unknown";
}

struct StepOutcome {
    StepResult result{StepResult::Ok};
    Cycle cycles_consumed{0};
    std::optional<Error> fault{std::nullopt};
    std::optional<CpuException> exception{std::nullopt};
    std::optional<UnsupportedFormInfo> unsupported_form{std::nullopt};
};

enum class RunStatus { Halted, Faulted, BudgetExhausted };

inline std::ostream& operator<<(std::ostream& os, RunStatus rs) {
    switch (rs) {
    case RunStatus::Halted:
        return os << "Halted";
    case RunStatus::Faulted:
        return os << "Faulted";
    case RunStatus::BudgetExhausted:
        return os << "BudgetExhausted";
    }
    return os << "Unknown";
}

struct RunOutcome {
    RunStatus status{RunStatus::Halted};
    u64 instructions_executed{0};
    Cycle cycles_consumed{0};
    std::optional<Error> fault{std::nullopt};
    std::optional<CpuException> exception{std::nullopt};
    std::optional<UnsupportedFormInfo> unsupported_form{std::nullopt};
};

class Cpu {
public:
    Cpu() = default;

    void Reset() noexcept;

    [[nodiscard]] const CpuContext& context() const noexcept { return context_; }
    [[nodiscard]] CpuContext& context() noexcept { return context_; }

    [[nodiscard]] CpuLifecycle lifecycle() const noexcept { return lifecycle_; }
    [[nodiscard]] const std::optional<Error>& last_fault() const noexcept { return last_fault_; }
    [[nodiscard]] const std::optional<CpuException>& last_exception() const noexcept {
        return last_exception_;
    }
    [[nodiscard]] const std::optional<UnsupportedFormInfo>& last_unsupported() const noexcept {
        return last_unsupported_;
    }

    void SetInterruptSource(IInterruptSource* source) noexcept { interrupt_source_ = source; }
    [[nodiscard]] IInterruptSource* interrupt_source() const noexcept { return interrupt_source_; }

    using TrapHandler = std::function<Result<bool>(u8 vector, CpuContext& ctx)>;
    void SetTrapHandler(TrapHandler handler) noexcept { trap_handler_ = std::move(handler); }
    [[nodiscard]] const TrapHandler& trap_handler() const noexcept { return trap_handler_; }

    [[nodiscard]] StepOutcome Step(const memory::AddressSpace& memory);
    [[nodiscard]] StepOutcome Step(memory::AddressSpace& memory);
    [[nodiscard]] StepOutcome Step(memory::VirtualMemory& memory);

    [[nodiscard]] RunOutcome RunWithBudget(const memory::AddressSpace& memory,
                                           u64 max_instructions = std::numeric_limits<u64>::max(),
                                           Cycle max_cycles = std::numeric_limits<Cycle>::max());
    [[nodiscard]] RunOutcome RunWithBudget(memory::AddressSpace& memory,
                                           u64 max_instructions = std::numeric_limits<u64>::max(),
                                           Cycle max_cycles = std::numeric_limits<Cycle>::max());
    [[nodiscard]] RunOutcome RunWithBudget(memory::VirtualMemory& memory,
                                           u64 max_instructions = std::numeric_limits<u64>::max(),
                                           Cycle max_cycles = std::numeric_limits<Cycle>::max());

    template <typename MemoryType>
    Result<void> RaiseException(CpuException ex, MemoryType& mem);

private:
    template <typename MemoryType>
    [[nodiscard]] StepOutcome StepGeneric(MemoryType& memory);

    template <typename MemoryType>
    [[nodiscard]] RunOutcome RunWithBudgetGeneric(MemoryType& memory, u64 max_instructions,
                                                  Cycle max_cycles);

    CpuContext context_{};
    CpuLifecycle lifecycle_{CpuLifecycle::Running};
    std::optional<Error> last_fault_{std::nullopt};
    std::optional<CpuException> last_exception_{std::nullopt};
    std::optional<UnsupportedFormInfo> last_unsupported_{std::nullopt};
    IInterruptSource* interrupt_source_{nullptr};
    TrapHandler trap_handler_{nullptr};
};

} // namespace xblob::cpu
