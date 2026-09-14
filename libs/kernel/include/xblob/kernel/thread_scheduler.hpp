#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/kernel/sync_objects.hpp"
#include "xblob/kernel/types.hpp"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace xblob::kernel {

enum class ThreadState : u8 {
    Ready = 0,
    Running = 1,
    Blocked = 2,
    Suspended = 3,
    Terminated = 4,
};

struct GuestThread {
    ThreadId id{kInvalidThreadId};
    ThreadState state{ThreadState::Ready};
    cpu::CpuContext context;
    GuestAddr stack_base{0};
    std::size_t stack_size{0};
    bool kernel_allocated_stack{false};
    std::vector<GuestHandle> wait_handles;
    bool wait_all{false};
    bool has_timeout{false};
    Cycle timeout_cycle{0};
    u32 wait_status{kStatusSuccess};
    u32 exit_code{0};
    u32 suspend_count{0};
    u8 priority{8};
};

class ThreadScheduler {
public:
    ThreadScheduler();

    void Reset();

    Result<ThreadId> CreateThread(GuestAddr entry_point, GuestAddr stack_top, u32 parameter,
                                  GuestAddr stack_base = 0, std::size_t stack_size = 0,
                                  bool kernel_allocated_stack = false);

    [[nodiscard]] ThreadId current_thread_id() const noexcept { return current_tid_; }
    [[nodiscard]] GuestThread* current_thread() noexcept;
    [[nodiscard]] const GuestThread* current_thread() const noexcept;
    [[nodiscard]] GuestThread* GetThread(ThreadId tid) noexcept;

    [[nodiscard]] std::size_t active_thread_count() const noexcept;
    [[nodiscard]] std::size_t ready_thread_count() const noexcept { return ready_queue_.size(); }

    Result<void> Yield(cpu::CpuContext& active_cpu_ctx);
    Result<void> BlockCurrent(GuestHandle handle, cpu::CpuContext& active_cpu_ctx);
    Result<void> BlockCurrentWithTimeout(const std::vector<GuestHandle>& handles, bool wait_all,
                                         bool has_timeout, Cycle timeout_cycle,
                                         cpu::CpuContext& active_cpu_ctx);

    Result<void> Wake(ThreadId tid, u32 wait_status = kStatusSuccess);
    Result<void> ExitCurrent(u32 exit_code, cpu::CpuContext& active_cpu_ctx);
    Result<void> TerminateThread(ThreadId tid, u32 exit_code);

    Result<u32> SuspendThread(ThreadId tid);
    Result<u32> ResumeThread(ThreadId tid);

    // Checks deadlines of blocked threads and wakes timed-out threads
    std::size_t CheckTimeouts(Cycle current_cycle, HandleTable& handles);

    // Switches active CPU context to the current/next ready thread
    Result<bool> ScheduleNext(cpu::CpuContext& active_cpu_ctx);

private:
    ThreadId next_tid_{1};
    ThreadId current_tid_{kInvalidThreadId};
    std::map<ThreadId, GuestThread> threads_;
    std::deque<ThreadId> ready_queue_;

    void EnqueueReady(ThreadId tid);
};

} // namespace xblob::kernel
