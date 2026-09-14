#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/core/scheduler.hpp"
#include "xblob/kernel/types.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace xblob::kernel {

// Clean-room deterministic timing constants (Xbox architecture: 733.333 MHz)
constexpr u64 kKernelCpuFrequencyHz = 733'333'333ULL;
constexpr u64 kKernelPerformanceCounterFreq = 733'333'333ULL;
constexpr u64 kCyclesPerMillisecond = 733'333ULL;
constexpr u64 kCyclesPer100Ns = 73ULL;                           // ~73.333 cycles per 100 ns
constexpr u64 kSystemTimeBaseEpoch100Ns = 126543744000000000ULL; // 2002-01-01 00:00:00 UTC

enum class TimerType : u8 {
    NotificationTimer = 0,    // Manual reset
    SynchronizationTimer = 1, // Auto reset on wait satisfied
};

class TimerObject {
public:
    explicit TimerObject(TimerType type = TimerType::SynchronizationTimer);

    [[nodiscard]] TimerType type() const noexcept { return type_; }
    [[nodiscard]] bool is_signaled() const noexcept { return signaled_; }
    [[nodiscard]] bool is_active() const noexcept { return is_active_; }
    [[nodiscard]] Cycle due_cycle() const noexcept { return due_cycle_; }
    [[nodiscard]] u32 period_ms() const noexcept { return period_ms_; }
    [[nodiscard]] GuestAddr dpc_routine() const noexcept { return dpc_routine_; }
    [[nodiscard]] u32 dpc_context() const noexcept { return dpc_context_; }

    void Set(Cycle due_cycle, u32 period_ms = 0, GuestAddr dpc = 0, u32 dpc_context = 0);
    bool Cancel(); // Returns true if timer was active

    bool SatisfyWait(ThreadId tid);

    void AddWaitingThread(ThreadId tid);
    void RemoveWaitingThread(ThreadId tid);
    [[nodiscard]] const std::deque<ThreadId>& waiting_threads() const noexcept {
        return waiting_threads_;
    }

    // Returns number of expirations triggered (0 or more)
    u64 AdvanceToCycle(Cycle current_cycle);

private:
    TimerType type_;
    bool signaled_{false};
    bool is_active_{false};
    Cycle due_cycle_{0};
    u32 period_ms_{0};
    GuestAddr dpc_routine_{0};
    u32 dpc_context_{0};
    std::deque<ThreadId> waiting_threads_;
};

class KernelTime {
public:
    explicit KernelTime(core::DeterministicScheduler* scheduler = nullptr);

    void SetScheduler(core::DeterministicScheduler* scheduler) noexcept { scheduler_ = scheduler; }
    [[nodiscard]] core::DeterministicScheduler* scheduler() noexcept { return scheduler_; }

    void SetCurrentCycle(Cycle cycle) noexcept { manual_cycles_ = cycle; }
    void AdvanceCycles(Cycle delta) noexcept;

    [[nodiscard]] Cycle current_cycle() const noexcept;
    [[nodiscard]] u64 QueryPerformanceCounter() const noexcept;
    [[nodiscard]] u64 QueryPerformanceFrequency() const noexcept {
        return kKernelPerformanceCounterFreq;
    }
    [[nodiscard]] u64 QuerySystemTime() const noexcept;
    [[nodiscard]] u32 QueryTickCount() const noexcept;

    // Conversion utilities
    [[nodiscard]] static constexpr u64 CycleToMillis(Cycle c) noexcept {
        return static_cast<u64>(c) / kCyclesPerMillisecond;
    }
    [[nodiscard]] static constexpr Cycle MillisToCycle(u64 ms) noexcept {
        return static_cast<Cycle>(ms * kCyclesPerMillisecond);
    }
    [[nodiscard]] static Cycle Relative100NsToCycles(i64 interval_100ns) noexcept;

    void Reset();

private:
    core::DeterministicScheduler* scheduler_{nullptr};
    Cycle manual_cycles_{0};
};

} // namespace xblob::kernel
