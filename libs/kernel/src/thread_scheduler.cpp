#include "xblob/kernel/thread_scheduler.hpp"

#include <algorithm>

namespace xblob::kernel {

ThreadScheduler::ThreadScheduler() = default;

void ThreadScheduler::Reset() {
    threads_.clear();
    ready_queue_.clear();
    current_tid_ = kInvalidThreadId;
    next_tid_ = 1;
}

Result<ThreadId> ThreadScheduler::CreateThread(GuestAddr entry_point, GuestAddr stack_top,
                                               u32 /*parameter*/, GuestAddr stack_base,
                                               std::size_t stack_size,
                                               bool kernel_allocated_stack) {
    ThreadId tid = next_tid_++;
    GuestThread thread;
    thread.id = tid;
    thread.state = ThreadState::Ready;
    thread.context.Reset();
    thread.context.eip = entry_point;
    thread.context.SetGpr(cpu::Reg32::ESP, stack_top);
    thread.stack_base = stack_base;
    thread.stack_size = stack_size;
    thread.kernel_allocated_stack = kernel_allocated_stack;
    thread.priority = 8;

    threads_[tid] = std::move(thread);

    if (current_tid_ == kInvalidThreadId) {
        current_tid_ = tid;
        threads_[tid].state = ThreadState::Running;
    } else {
        EnqueueReady(tid);
    }

    return tid;
}

GuestThread* ThreadScheduler::current_thread() noexcept {
    auto it = threads_.find(current_tid_);
    if (it != threads_.end())
        return &it->second;
    return nullptr;
}

const GuestThread* ThreadScheduler::current_thread() const noexcept {
    auto it = threads_.find(current_tid_);
    if (it != threads_.end())
        return &it->second;
    return nullptr;
}

GuestThread* ThreadScheduler::GetThread(ThreadId tid) noexcept {
    auto it = threads_.find(tid);
    if (it != threads_.end())
        return &it->second;
    return nullptr;
}

std::size_t ThreadScheduler::active_thread_count() const noexcept {
    std::size_t count = 0;
    for (const auto& [_, t] : threads_) {
        if (t.state != ThreadState::Terminated)
            ++count;
    }
    return count;
}

void ThreadScheduler::EnqueueReady(ThreadId tid) {
    if (std::find(ready_queue_.begin(), ready_queue_.end(), tid) == ready_queue_.end()) {
        ready_queue_.push_back(tid);
    }
}

Result<void> ThreadScheduler::Yield(cpu::CpuContext& active_cpu_ctx) {
    if (auto* curr = current_thread()) {
        curr->context = active_cpu_ctx;
        if (curr->state == ThreadState::Running) {
            curr->state = ThreadState::Ready;
            EnqueueReady(curr->id);
        }
    }
    (void)ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::BlockCurrent(GuestHandle handle, cpu::CpuContext& active_cpu_ctx) {
    std::vector<GuestHandle> h_vec;
    if (handle != kInvalidHandle) {
        h_vec.push_back(handle);
    }
    return BlockCurrentWithTimeout(h_vec, false, false, 0, active_cpu_ctx);
}

Result<void> ThreadScheduler::BlockCurrentWithTimeout(const std::vector<GuestHandle>& handles,
                                                      bool wait_all, bool has_timeout,
                                                      Cycle timeout_cycle,
                                                      cpu::CpuContext& active_cpu_ctx) {
    auto* curr = current_thread();
    if (!curr) {
        return Error{ErrorCode::InvalidState, "Nenhuma thread ativa para bloquear"};
    }
    curr->context = active_cpu_ctx;
    curr->state = ThreadState::Blocked;
    curr->wait_handles = handles;
    curr->wait_all = wait_all;
    curr->has_timeout = has_timeout;
    curr->timeout_cycle = timeout_cycle;
    (void)ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::Wake(ThreadId tid, u32 wait_status) {
    auto* thread = GetThread(tid);
    if (!thread) {
        return Error{ErrorCode::InvalidArgument, "Thread inválida"};
    }
    if (thread->state == ThreadState::Blocked) {
        thread->state = ThreadState::Ready;
        thread->wait_handles.clear();
        thread->has_timeout = false;
        thread->wait_status = wait_status;
        thread->context.SetGpr(cpu::Reg32::EAX, wait_status);
        EnqueueReady(tid);
    }
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::ExitCurrent(u32 exit_code, cpu::CpuContext& active_cpu_ctx) {
    auto* curr = current_thread();
    if (!curr) {
        return Error{ErrorCode::InvalidState, "Nenhuma thread ativa para encerrar"};
    }
    curr->state = ThreadState::Terminated;
    curr->exit_code = exit_code;
    (void)ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::TerminateThread(ThreadId tid, u32 exit_code) {
    auto* thread = GetThread(tid);
    if (!thread) {
        return Error{ErrorCode::InvalidArgument, "Thread inválida para terminação"};
    }
    thread->state = ThreadState::Terminated;
    thread->exit_code = exit_code;
    ready_queue_.erase(std::remove(ready_queue_.begin(), ready_queue_.end(), tid),
                       ready_queue_.end());
    return Result<void>::Ok();
}

Result<u32> ThreadScheduler::SuspendThread(ThreadId tid) {
    auto* thread = GetThread(tid);
    if (!thread || thread->state == ThreadState::Terminated) {
        return Error{ErrorCode::InvalidArgument, "Thread inválida ou terminada"};
    }
    u32 prev = thread->suspend_count++;
    if (thread->state == ThreadState::Ready) {
        thread->state = ThreadState::Suspended;
        ready_queue_.erase(std::remove(ready_queue_.begin(), ready_queue_.end(), tid),
                           ready_queue_.end());
    }
    return prev;
}

Result<u32> ThreadScheduler::ResumeThread(ThreadId tid) {
    auto* thread = GetThread(tid);
    if (!thread || thread->state == ThreadState::Terminated) {
        return Error{ErrorCode::InvalidArgument, "Thread inválida ou terminada"};
    }
    if (thread->suspend_count == 0) {
        return 0;
    }
    u32 prev = thread->suspend_count--;
    if (thread->suspend_count == 0 && thread->state == ThreadState::Suspended) {
        thread->state = ThreadState::Ready;
        EnqueueReady(tid);
    }
    return prev;
}

std::size_t ThreadScheduler::CheckTimeouts(Cycle current_cycle, HandleTable& handles) {
    std::size_t awakened = 0;
    for (auto& [tid, thread] : threads_) {
        if (thread.state == ThreadState::Blocked && thread.has_timeout &&
            current_cycle >= thread.timeout_cycle) {
            for (GuestHandle h : thread.wait_handles) {
                handles.RemoveWaitingThread(h, tid);
            }
            thread.state = ThreadState::Ready;
            thread.wait_handles.clear();
            thread.has_timeout = false;
            thread.wait_status = kStatusWaitTimeout;
            thread.context.SetGpr(cpu::Reg32::EAX, kStatusWaitTimeout);
            EnqueueReady(tid);
            awakened++;
        }
    }
    return awakened;
}

Result<bool> ThreadScheduler::ScheduleNext(cpu::CpuContext& active_cpu_ctx) {
    if (ready_queue_.empty()) {
        return false;
    }

    ThreadId next_tid = ready_queue_.front();
    ready_queue_.pop_front();

    auto it = threads_.find(next_tid);
    if (it == threads_.end() || it->second.state == ThreadState::Terminated ||
        it->second.state == ThreadState::Suspended) {
        return ScheduleNext(active_cpu_ctx);
    }

    current_tid_ = next_tid;
    it->second.state = ThreadState::Running;
    active_cpu_ctx = it->second.context;
    return true;
}

} // namespace xblob::kernel
