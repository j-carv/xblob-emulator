#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
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
    Terminated = 3,
};

struct GuestThread {
    ThreadId id{kInvalidThreadId};
    ThreadState state{ThreadState::Ready};
    cpu::CpuContext context;
    GuestAddr stack_base{0};
    std::size_t stack_size{0};
    GuestHandle wait_handle{kInvalidHandle};
    u32 exit_code{0};
};

class ThreadScheduler {
public:
    ThreadScheduler();

    void Reset();

    Result<ThreadId> CreateThread(GuestAddr entry_point, GuestAddr stack_top, u32 parameter,
                                  GuestAddr stack_base = 0, std::size_t stack_size = 0);

    [[nodiscard]] ThreadId current_thread_id() const noexcept { return current_tid_; }
    [[nodiscard]] GuestThread* current_thread() noexcept;
    [[nodiscard]] const GuestThread* current_thread() const noexcept;
    [[nodiscard]] GuestThread* GetThread(ThreadId tid) noexcept;

    [[nodiscard]] std::size_t active_thread_count() const noexcept;

    Result<void> Yield(cpu::CpuContext& active_cpu_ctx);
    Result<void> BlockCurrent(GuestHandle handle, cpu::CpuContext& active_cpu_ctx);
    Result<void> Wake(ThreadId tid);
    Result<void> ExitCurrent(u32 exit_code, cpu::CpuContext& active_cpu_ctx);

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
