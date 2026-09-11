#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/kernel/types.hpp"

#include <deque>
#include <optional>
#include <vector>

namespace xblob::kernel {

class EventObject {
public:
    explicit EventObject(bool manual_reset = false, bool initial_state = false);

    [[nodiscard]] bool manual_reset() const noexcept { return manual_reset_; }
    [[nodiscard]] bool is_signaled() const noexcept { return signaled_; }

    void Set();
    void Reset();
    bool SatisfyWait(ThreadId tid);

    void AddWaitingThread(ThreadId tid);
    void RemoveWaitingThread(ThreadId tid);
    [[nodiscard]] const std::deque<ThreadId>& waiting_threads() const noexcept {
        return waiting_threads_;
    }

private:
    bool manual_reset_;
    bool signaled_;
    std::deque<ThreadId> waiting_threads_;
};

class MutexObject {
public:
    explicit MutexObject(bool initial_owner = false, ThreadId owner_tid = kInvalidThreadId);

    [[nodiscard]] bool is_locked() const noexcept { return recursion_count_ > 0; }
    [[nodiscard]] ThreadId owner() const noexcept { return owner_tid_; }
    [[nodiscard]] u32 recursion_count() const noexcept { return recursion_count_; }

    Result<bool> Acquire(ThreadId tid);
    Result<void> Release(ThreadId tid);

    void AddWaitingThread(ThreadId tid);
    void RemoveWaitingThread(ThreadId tid);
    [[nodiscard]] const std::deque<ThreadId>& waiting_threads() const noexcept {
        return waiting_threads_;
    }

private:
    ThreadId owner_tid_{kInvalidThreadId};
    u32 recursion_count_{0};
    std::deque<ThreadId> waiting_threads_;
};

class HandleTable {
public:
    HandleTable();

    [[nodiscard]] Result<GuestHandle> AllocateEvent(bool manual_reset, bool initial_state);
    [[nodiscard]] Result<GuestHandle> AllocateMutex(bool initial_owner, ThreadId owner_tid);
    [[nodiscard]] Result<GuestHandle> AllocateThreadHandle(ThreadId tid);

    [[nodiscard]] Result<EventObject*> GetEvent(GuestHandle handle);
    [[nodiscard]] Result<MutexObject*> GetMutex(GuestHandle handle);
    [[nodiscard]] Result<ThreadId> GetThreadId(GuestHandle handle);

    [[nodiscard]] Result<void> CloseHandle(GuestHandle handle);

    void Reset();

private:
    struct Slot {
        HandleEntry entry;
        bool in_use{false};
    };

    std::vector<Slot> slots_;
    std::vector<EventObject> events_;
    std::vector<MutexObject> mutexes_;

    [[nodiscard]] Result<std::size_t> ValidateAndGetSlot(GuestHandle handle,
                                                         HandleType expected_type) const;
};

} // namespace xblob::kernel
