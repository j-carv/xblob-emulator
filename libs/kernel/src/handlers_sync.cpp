#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

void RegisterSyncExports(ExportRegistry& reg) {
    // Ordinal 64: CreateEvent (4 parameters: Security, ManualReset, InitialState, Name)
    (void)reg.RegisterExport(64, "CreateEvent", 4, [](GuestContext& ctx) -> Result<u32> {
        auto manual = ctx.ReadArg(1);
        auto initial = ctx.ReadArg(2);
        bool m = manual ? (*manual != 0) : false;
        bool init = initial ? (*initial != 0) : false;
        auto handle_res = ctx.kernel.handles().AllocateEvent(m, init);
        if (!handle_res)
            return kInvalidHandle;
        return *handle_res;
    });

    // Ordinal 65: SetEvent (1 parameter: EventHandle)
    (void)reg.RegisterExport(65, "SetEvent", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto ev = ctx.kernel.handles().GetEvent(*h);
        if (!ev)
            return 0;
        (*ev)->Set();

        // FIFO wake: if manual reset, wake all; if auto reset, wake first that satisfies
        if ((*ev)->manual_reset()) {
            std::vector<ThreadId> to_wake;
            for (ThreadId tid : (*ev)->waiting_threads()) {
                to_wake.push_back(tid);
            }
            for (ThreadId tid : to_wake) {
                (*ev)->RemoveWaitingThread(tid);
                (void)ctx.kernel.threads().Wake(tid, kStatusWait0);
            }
        } else {
            if (!(*ev)->waiting_threads().empty()) {
                ThreadId first = (*ev)->waiting_threads().front();
                (*ev)->RemoveWaitingThread(first);
                (*ev)->Reset();
                (void)ctx.kernel.threads().Wake(first, kStatusWait0);
            }
        }
        return 1;
    });

    // Ordinal 66: ResetEvent (1 parameter: EventHandle)
    (void)reg.RegisterExport(66, "ResetEvent", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto ev = ctx.kernel.handles().GetEvent(*h);
        if (!ev)
            return 0;
        (*ev)->Reset();
        return 1;
    });

    // Ordinal 67: PulseEvent (1 parameter: EventHandle)
    (void)reg.RegisterExport(67, "PulseEvent", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto ev = ctx.kernel.handles().GetEvent(*h);
        if (!ev)
            return 0;

        if ((*ev)->manual_reset()) {
            std::vector<ThreadId> to_wake;
            for (ThreadId tid : (*ev)->waiting_threads()) {
                to_wake.push_back(tid);
            }
            for (ThreadId tid : to_wake) {
                (*ev)->RemoveWaitingThread(tid);
                (void)ctx.kernel.threads().Wake(tid, kStatusWait0);
            }
        } else if (!(*ev)->waiting_threads().empty()) {
            ThreadId first = (*ev)->waiting_threads().front();
            (*ev)->RemoveWaitingThread(first);
            (void)ctx.kernel.threads().Wake(first, kStatusWait0);
        }
        (*ev)->Reset();
        return 1;
    });

    // Ordinal 80: CreateMutex (3 parameters: Security, InitialOwner, Name)
    (void)reg.RegisterExport(80, "CreateMutex", 3, [](GuestContext& ctx) -> Result<u32> {
        auto initial_owner = ctx.ReadArg(1);
        bool init = initial_owner ? (*initial_owner != 0) : false;
        ThreadId owner_tid = init ? ctx.kernel.threads().current_thread_id() : kInvalidThreadId;
        auto h_res = ctx.kernel.handles().AllocateMutex(init, owner_tid);
        if (!h_res)
            return kInvalidHandle;
        return *h_res;
    });

    // Ordinal 84: ReleaseMutex (1 parameter: MutexHandle)
    (void)reg.RegisterExport(84, "ReleaseMutex", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto m = ctx.kernel.handles().GetMutex(*h);
        if (!m)
            return 0;
        auto rel_res = (*m)->Release(ctx.kernel.threads().current_thread_id());
        if (!rel_res)
            return 0;

        // If mutant is now completely unlocked, wake first waiting thread in FIFO order
        if (!(*m)->is_locked() && !(*m)->waiting_threads().empty()) {
            ThreadId first = (*m)->waiting_threads().front();
            (*m)->RemoveWaitingThread(first);
            (void)(*m)->Acquire(first);
            (void)ctx.kernel.threads().Wake(first, kStatusWait0);
        }
        return 1;
    });

    // Ordinal 75: CreateSemaphore (4 parameters: Security, InitialCount, MaxCount, Name)
    (void)reg.RegisterExport(75, "CreateSemaphore", 4, [](GuestContext& ctx) -> Result<u32> {
        auto init_count = ctx.ReadArg(1);
        auto max_count = ctx.ReadArg(2);
        if (!init_count || !max_count) {
            return kInvalidHandle;
        }
        auto h_res = ctx.kernel.handles().AllocateSemaphore(static_cast<i32>(*init_count),
                                                            static_cast<i32>(*max_count));
        if (!h_res) {
            return kInvalidHandle;
        }
        return *h_res;
    });

    // Ordinal 76: ReleaseSemaphore (4 parameters: Handle, ReleaseCount, PrevCountPtr, Ignored)
    (void)reg.RegisterExport(76, "ReleaseSemaphore", 4, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto rel_count = ctx.ReadArg(1);
        auto prev_ptr = ctx.ReadArg(2);
        if (!h || !rel_count) {
            return 0;
        }

        auto sem_res = ctx.kernel.handles().GetSemaphore(*h);
        if (!sem_res) {
            return 0;
        }

        i32 prev = 0;
        auto rel_res = (*sem_res)->Release(static_cast<i32>(*rel_count), &prev);
        if (!rel_res) {
            return 0;
        }

        if (prev_ptr && *prev_ptr != 0) {
            (void)ctx.Write32(*prev_ptr, static_cast<u32>(prev));
        }

        // Wake waiting threads in FIFO order up to release_count
        u32 wake_budget = *rel_count;
        while (wake_budget > 0 && !(*sem_res)->waiting_threads().empty()) {
            ThreadId tid = (*sem_res)->waiting_threads().front();
            (*sem_res)->RemoveWaitingThread(tid);
            (void)(*sem_res)->Acquire(tid);
            (void)ctx.kernel.threads().Wake(tid, kStatusWait0);
            wake_budget--;
        }

        return 1;
    });

    // Ordinal 90: WaitForSingleObject (2 parameters: Handle, TimeoutMs)
    (void)reg.RegisterExport(90, "WaitForSingleObject", 2, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto timeout = ctx.ReadArg(1);
        if (!h) {
            return kStatusInvalidHandle;
        }

        GuestHandle handle = *h;
        if (!ctx.kernel.handles().IsValidHandle(handle)) {
            return kStatusInvalidHandle;
        }

        ThreadId tid = ctx.kernel.threads().current_thread_id();
        if (ctx.kernel.handles().SatisfyWait(handle, tid)) {
            return kStatusWait0;
        }

        u32 timeout_ms = timeout ? *timeout : 0xFFFFFFFFU;
        if (timeout_ms == 0) {
            return kStatusWaitTimeout;
        }

        bool has_timeout = (timeout_ms != 0xFFFFFFFFU);
        Cycle deadline = 0;
        if (has_timeout) {
            deadline = ctx.kernel.time().current_cycle() + KernelTime::MillisToCycle(timeout_ms);
        }

        ctx.kernel.handles().AddWaitingThread(handle, tid);
        (void)ctx.kernel.threads().BlockCurrentWithTimeout({handle}, false, has_timeout, deadline,
                                                           ctx.cpu);
        return kStatusWait0;
    });

    // Ordinal 91: WaitForMultipleObjects (4 parameters: Count, HandlesPtr, WaitAll, TimeoutMs)
    (void)reg.RegisterExport(91, "WaitForMultipleObjects", 4, [](GuestContext& ctx) -> Result<u32> {
        auto count_arg = ctx.ReadArg(0);
        auto handles_ptr = ctx.ReadArg(1);
        auto wait_all_arg = ctx.ReadArg(2);
        auto timeout_arg = ctx.ReadArg(3);
        if (!count_arg || !handles_ptr) {
            return kStatusInvalidParameter;
        }

        u32 count = *count_arg;
        if (count == 0 || count > 64) {
            return kStatusInvalidParameter;
        }

        // Step 1: Read and validate ALL handles atomically
        std::vector<GuestHandle> handles(count);
        for (u32 i = 0; i < count; ++i) {
            auto h_read = ctx.Read32(*handles_ptr + (i * 4));
            if (!h_read) {
                return kStatusAccessViolation;
            }
            GuestHandle h = *h_read;
            if (!ctx.kernel.handles().IsValidHandle(h)) {
                return kStatusInvalidHandle; // Atomic transactional rejection
            }
            handles[i] = h;
        }

        ThreadId tid = ctx.kernel.threads().current_thread_id();
        bool wait_all = wait_all_arg ? (*wait_all_arg != 0) : false;

        if (!wait_all) {
            // WaitAny: Check if any object is signaled
            for (u32 i = 0; i < count; ++i) {
                if (ctx.kernel.handles().SatisfyWait(handles[i], tid)) {
                    return kStatusWait0 + i;
                }
            }
        } else {
            // WaitAll: Check if ALL objects are signaled
            bool all_signaled = true;
            for (u32 i = 0; i < count; ++i) {
                if (!ctx.kernel.handles().IsObjectSignaled(handles[i], tid)) {
                    all_signaled = false;
                    break;
                }
            }
            if (all_signaled) {
                for (u32 i = 0; i < count; ++i) {
                    (void)ctx.kernel.handles().SatisfyWait(handles[i], tid);
                }
                return kStatusWait0;
            }
        }

        u32 timeout_ms = timeout_arg ? *timeout_arg : 0xFFFFFFFFU;
        if (timeout_ms == 0) {
            return kStatusWaitTimeout;
        }

        bool has_timeout = (timeout_ms != 0xFFFFFFFFU);
        Cycle deadline = 0;
        if (has_timeout) {
            deadline = ctx.kernel.time().current_cycle() + KernelTime::MillisToCycle(timeout_ms);
        }

        for (GuestHandle h : handles) {
            ctx.kernel.handles().AddWaitingThread(h, tid);
        }
        (void)ctx.kernel.threads().BlockCurrentWithTimeout(handles, wait_all, has_timeout, deadline,
                                                           ctx.cpu);
        return kStatusWait0;
    });
}

} // namespace xblob::kernel
