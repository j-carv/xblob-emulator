#include "xblob/kernel/kernel_time.hpp"

#include <algorithm>

namespace xblob::kernel {

TimerObject::TimerObject(TimerType type) : type_(type) {}

void TimerObject::Set(Cycle due_cycle, u32 period_ms, GuestAddr dpc, u32 dpc_context) {
    due_cycle_ = due_cycle;
    period_ms_ = period_ms;
    dpc_routine_ = dpc;
    dpc_context_ = dpc_context;
    signaled_ = false;
    is_active_ = true;
}

bool TimerObject::Cancel() {
    bool was = is_active_;
    is_active_ = false;
    signaled_ = false;
    return was;
}

bool TimerObject::SatisfyWait(ThreadId /*tid*/) {
    if (signaled_) {
        if (type_ == TimerType::SynchronizationTimer) {
            signaled_ = false;
        }
        return true;
    }
    return false;
}

void TimerObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void TimerObject::RemoveWaitingThread(ThreadId tid) {
    waiting_threads_.erase(std::remove(waiting_threads_.begin(), waiting_threads_.end(), tid),
                           waiting_threads_.end());
}

u64 TimerObject::AdvanceToCycle(Cycle current_cycle) {
    if (!is_active_ || current_cycle < due_cycle_) {
        return 0;
    }

    signaled_ = true;

    if (period_ms_ == 0) {
        // One-shot timer
        is_active_ = false;
        return 1;
    }

    Cycle period_cycles = KernelTime::MillisToCycle(period_ms_);
    if (period_cycles == 0) {
        period_cycles = 1;
    }

    Cycle elapsed = current_cycle - due_cycle_;
    u64 count = static_cast<u64>(elapsed / period_cycles) + 1;
    due_cycle_ += static_cast<Cycle>(count * period_cycles);
    return count;
}

KernelTime::KernelTime(core::DeterministicScheduler* scheduler) : scheduler_(scheduler) {}

void KernelTime::AdvanceCycles(Cycle delta) noexcept {
    if (scheduler_) {
        (void)scheduler_->AdvanceCycles(delta);
    } else {
        manual_cycles_ += delta;
    }
}

Cycle KernelTime::current_cycle() const noexcept {
    if (scheduler_) {
        return scheduler_->current_cycle();
    }
    return manual_cycles_;
}

u64 KernelTime::QueryPerformanceCounter() const noexcept {
    return static_cast<u64>(current_cycle());
}

u64 KernelTime::QuerySystemTime() const noexcept {
    Cycle cyc = current_cycle();
    u64 intervals = (static_cast<u64>(cyc) * 10'000'000ULL) / kKernelCpuFrequencyHz;
    return kSystemTimeBaseEpoch100Ns + intervals;
}

u32 KernelTime::QueryTickCount() const noexcept {
    return static_cast<u32>(current_cycle() / kCyclesPerMillisecond);
}

Cycle KernelTime::Relative100NsToCycles(i64 interval_100ns) noexcept {
    u64 abs_val =
        interval_100ns < 0 ? static_cast<u64>(-interval_100ns) : static_cast<u64>(interval_100ns);
    return static_cast<Cycle>((abs_val * kKernelCpuFrequencyHz) / 10'000'000ULL);
}

void KernelTime::Reset() {
    manual_cycles_ = 0;
}

} // namespace xblob::kernel
