#pragma once

#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/machine/trace_buffer.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace xblob::machine {

enum class StopReasonCode : u8 {
    None = 0,
    Paused = 1,
    StepCompleted = 2,
    Halted = 3,
    BudgetInstructionsExhausted = 4,
    BudgetCyclesExhausted = 5,
    BudgetWallTimeExhausted = 6,
    BudgetEventsExhausted = 7,
    WatchdogTimeout = 8,
    UnsupportedOpcode = 9,
    UnsupportedExport = 10,
    UnsupportedGpuMethod = 11,
    UnsupportedFileService = 12,
    CpuException = 13,
    MemoryFault = 14,
    InternalError = 15,
};

[[nodiscard]] constexpr const char* ToString(StopReasonCode code) noexcept {
    switch (code) {
    case StopReasonCode::None:
        return "None";
    case StopReasonCode::Paused:
        return "Paused";
    case StopReasonCode::StepCompleted:
        return "StepCompleted";
    case StopReasonCode::Halted:
        return "Halted";
    case StopReasonCode::BudgetInstructionsExhausted:
        return "BudgetInstructionsExhausted";
    case StopReasonCode::BudgetCyclesExhausted:
        return "BudgetCyclesExhausted";
    case StopReasonCode::BudgetWallTimeExhausted:
        return "BudgetWallTimeExhausted";
    case StopReasonCode::BudgetEventsExhausted:
        return "BudgetEventsExhausted";
    case StopReasonCode::WatchdogTimeout:
        return "WatchdogTimeout";
    case StopReasonCode::UnsupportedOpcode:
        return "UnsupportedOpcode";
    case StopReasonCode::UnsupportedExport:
        return "UnsupportedExport";
    case StopReasonCode::UnsupportedGpuMethod:
        return "UnsupportedGpuMethod";
    case StopReasonCode::UnsupportedFileService:
        return "UnsupportedFileService";
    case StopReasonCode::CpuException:
        return "CpuException";
    case StopReasonCode::MemoryFault:
        return "MemoryFault";
    case StopReasonCode::InternalError:
        return "InternalError";
    }
    return "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, StopReasonCode code) {
    return os << ToString(code);
}

struct StopReason {
    StopReasonCode code{StopReasonCode::None};
    GuestAddr fault_eip{0};
    u32 thread_id{0};
    u32 ordinal_or_opcode{0};
    u64 count{0};
    std::string category;
    std::string symbol_or_mnemonic;
    std::string detail;
};

struct ExecutionBudgets {
    u64 max_instructions{std::numeric_limits<u64>::max()};
    u64 max_cycles{std::numeric_limits<Cycle>::max()};
    u64 max_wall_time_ms{0}; // 0 = unlimited
    u64 max_events{std::numeric_limits<u64>::max()};
    u32 chunk_instructions{1000};
};

struct MachineSnapshot {
    u8 state_val{0};
    StopReason last_stop_reason{};
    cpu::CpuContext cpu_context{};
    u32 eflags_raw{0};
    Cycle current_cycle{0};
    u64 instructions_executed{0};
    u64 events_fired{0};
    u32 active_thread_id{0};
    u32 thread_count{0};
    bool stack_valid{false};
    std::vector<u32> stack_words{}; // Up to 8 validated stack words
    std::string error_message;
};

struct CompatibilityDiagnostic {
    StopReason first_blocker{};
    u64 total_instructions{0};
    u64 total_cycles{0};
    std::vector<TraceEvent> recent_trace{};
};

} // namespace xblob::machine
