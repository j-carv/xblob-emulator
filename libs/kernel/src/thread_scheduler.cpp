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
                                               std::size_t stack_size) {
    ThreadId tid = next_tid_++;
    GuestThread thread;
    thread.id = tid;
    thread.state = ThreadState::Ready;
    thread.context.Reset();
    thread.context.eip = entry_point;
    thread.context.SetGpr(cpu::Reg32::ESP, stack_top);
    thread.stack_base = stack_base;
    thread.stack_size = stack_size;

    // Parameter in stdcall: could be in register or passed on stack
    // By convention in thread entry, push parameter and fake return address
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
    ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::BlockCurrent(GuestHandle handle, cpu::CpuContext& active_cpu_ctx) {
    auto* curr = current_thread();
    if (!curr) {
        return Error{ErrorCode::InvalidState, "Nenhuma thread ativa para bloquear"};
    }
    curr->context = active_cpu_ctx;
    curr->state = ThreadState::Blocked;
    curr->wait_handle = handle;
    ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<void> ThreadScheduler::Wake(ThreadId tid) {
    auto* thread = GetThread(tid);
    if (!thread) {
        return Error{ErrorCode::InvalidArgument, "Thread inválida"};
    }
    if (thread->state == ThreadState::Blocked) {
        thread->state = ThreadState::Ready;
        thread->wait_handle = kInvalidHandle;
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
    ScheduleNext(active_cpu_ctx);
    return Result<void>::Ok();
}

Result<bool> ThreadScheduler::ScheduleNext(cpu::CpuContext& active_cpu_ctx) {
    if (ready_queue_.empty()) {
        // No threads ready
        return false;
    }

    ThreadId next_tid = ready_queue_.front();
    ready_queue_.pop_front();

    auto it = threads_.find(next_tid);
    if (it == threads_.end() || it->second.state == ThreadState::Terminated) {
        return ScheduleNext(active_cpu_ctx);
    }

    current_tid_ = next_tid;
    it->second.state = ThreadState::Running;
    active_cpu_ctx = it->second.context;
    return true;
}

} // namespace xblob::kernel
