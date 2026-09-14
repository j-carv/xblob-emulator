#include "xblob/kernel/sync_objects.hpp"

#include <algorithm>

namespace xblob::kernel {

EventObject::EventObject(bool manual_reset, bool initial_state)
    : manual_reset_(manual_reset), signaled_(initial_state) {}

void EventObject::Set() {
    signaled_ = true;
}

void EventObject::Reset() {
    signaled_ = false;
}

bool EventObject::SatisfyWait(ThreadId /*tid*/) {
    if (signaled_) {
        if (!manual_reset_) {
            signaled_ = false;
        }
        return true;
    }
    return false;
}

void EventObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void EventObject::RemoveWaitingThread(ThreadId tid) {
    waiting_threads_.erase(std::remove(waiting_threads_.begin(), waiting_threads_.end(), tid),
                           waiting_threads_.end());
}

MutexObject::MutexObject(bool initial_owner, ThreadId owner_tid)
    : owner_tid_(initial_owner ? owner_tid : kInvalidThreadId),
      recursion_count_(initial_owner ? 1 : 0) {}

Result<bool> MutexObject::Acquire(ThreadId tid) {
    if (recursion_count_ == 0) {
        owner_tid_ = tid;
        recursion_count_ = 1;
        return true;
    }
    if (owner_tid_ == tid) {
        recursion_count_++;
        return true;
    }
    return false;
}

Result<void> MutexObject::Release(ThreadId tid) {
    if (owner_tid_ != tid || recursion_count_ == 0) {
        return Error{ErrorCode::InvalidArgument, "Mutant não pertence à thread chamadora"};
    }
    recursion_count_--;
    if (recursion_count_ == 0) {
        owner_tid_ = kInvalidThreadId;
    }
    return Result<void>::Ok();
}

bool MutexObject::SatisfyWait(ThreadId tid) {
    auto res = Acquire(tid);
    return res.has_value() && *res;
}

void MutexObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void MutexObject::RemoveWaitingThread(ThreadId tid) {
    waiting_threads_.erase(std::remove(waiting_threads_.begin(), waiting_threads_.end(), tid),
                           waiting_threads_.end());
}

SemaphoreObject::SemaphoreObject(i32 initial_count, i32 maximum_count)
    : current_count_(initial_count), maximum_count_(maximum_count > 0 ? maximum_count : 1) {}

Result<bool> SemaphoreObject::Acquire(ThreadId /*tid*/) {
    if (current_count_ > 0) {
        current_count_--;
        return true;
    }
    return false;
}

Result<void> SemaphoreObject::Release(i32 release_count, i32* previous_count) {
    if (release_count <= 0) {
        return Error{ErrorCode::InvalidArgument, "Release count deve ser maior que zero"};
    }
    if (current_count_ + release_count > maximum_count_) {
        return Error{ErrorCode::LimitReached, "Limite máximo do semáforo excedido"};
    }
    if (previous_count) {
        *previous_count = current_count_;
    }
    current_count_ += release_count;
    return Result<void>::Ok();
}

bool SemaphoreObject::SatisfyWait(ThreadId tid) {
    auto res = Acquire(tid);
    return res.has_value() && *res;
}

void SemaphoreObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void SemaphoreObject::RemoveWaitingThread(ThreadId tid) {
    waiting_threads_.erase(std::remove(waiting_threads_.begin(), waiting_threads_.end(), tid),
                           waiting_threads_.end());
}

HandleTable::HandleTable() = default;

void HandleTable::Reset() {
    slots_.clear();
    events_.clear();
    mutexes_.clear();
    semaphores_.clear();
    timers_.clear();
}

Result<GuestHandle> HandleTable::AllocateEvent(bool manual_reset, bool initial_state) {
    events_.emplace_back(manual_reset, initial_state);
    u32 obj_id = static_cast<u32>(events_.size() - 1);

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Event;
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }

    Slot s;
    s.in_use = true;
    s.entry.type = HandleType::Event;
    s.entry.generation = 1;
    s.entry.object_id = obj_id;
    slots_.push_back(s);
    return EncodeHandle(static_cast<u16>(slots_.size() - 1), 1);
}

Result<GuestHandle> HandleTable::AllocateMutex(bool initial_owner, ThreadId owner_tid) {
    mutexes_.emplace_back(initial_owner, owner_tid);
    u32 obj_id = static_cast<u32>(mutexes_.size() - 1);

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Mutex;
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }

    Slot s;
    s.in_use = true;
    s.entry.type = HandleType::Mutex;
    s.entry.generation = 1;
    s.entry.object_id = obj_id;
    slots_.push_back(s);
    return EncodeHandle(static_cast<u16>(slots_.size() - 1), 1);
}

Result<GuestHandle> HandleTable::AllocateSemaphore(i32 initial_count, i32 max_count) {
    if (initial_count < 0 || max_count <= 0 || initial_count > max_count) {
        return Error{ErrorCode::InvalidArgument, "Parâmetros de semáforo inválidos"};
    }
    semaphores_.emplace_back(initial_count, max_count);
    u32 obj_id = static_cast<u32>(semaphores_.size() - 1);

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Semaphore;
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }

    Slot s;
    s.in_use = true;
    s.entry.type = HandleType::Semaphore;
    s.entry.generation = 1;
    s.entry.object_id = obj_id;
    slots_.push_back(s);
    return EncodeHandle(static_cast<u16>(slots_.size() - 1), 1);
}

Result<GuestHandle> HandleTable::AllocateTimer(TimerType type) {
    timers_.emplace_back(type);
    u32 obj_id = static_cast<u32>(timers_.size() - 1);

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Timer;
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }

    Slot s;
    s.in_use = true;
    s.entry.type = HandleType::Timer;
    s.entry.generation = 1;
    s.entry.object_id = obj_id;
    slots_.push_back(s);
    return EncodeHandle(static_cast<u16>(slots_.size() - 1), 1);
}

Result<GuestHandle> HandleTable::AllocateThreadHandle(ThreadId tid) {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Thread;
            slots_[i].entry.object_id = tid;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }

    Slot s;
    s.in_use = true;
    s.entry.type = HandleType::Thread;
    s.entry.generation = 1;
    s.entry.object_id = tid;
    slots_.push_back(s);
    return EncodeHandle(static_cast<u16>(slots_.size() - 1), 1);
}

Result<std::size_t> HandleTable::ValidateAndGetSlot(GuestHandle handle,
                                                    HandleType expected_type) const {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res) {
        return slot_res.error();
    }
    std::size_t idx = *slot_res;
    if (slots_[idx].entry.type != expected_type) {
        return Error{ErrorCode::InvalidArgument, "Tipo de handle divergente do esperado", handle};
    }
    return idx;
}

Result<std::size_t> HandleTable::ValidateSlotOnly(GuestHandle handle) const {
    if (handle == kInvalidHandle) {
        return Error{ErrorCode::InvalidArgument, "Handle nulo ou inválido", handle};
    }
    u16 idx = GetHandleIndex(handle);
    u16 gen = GetHandleGeneration(handle);

    if (idx >= slots_.size()) {
        return Error{ErrorCode::OutOfBounds, "Índice de handle fora dos limites", handle};
    }
    const auto& s = slots_[idx];
    if (!s.in_use || s.entry.generation != gen) {
        return Error{ErrorCode::InvalidArgument, "Handle stale ou reutilizado", handle};
    }
    return static_cast<std::size_t>(idx);
}

bool HandleTable::IsValidHandle(GuestHandle handle) const noexcept {
    return ValidateSlotOnly(handle).has_value();
}

Result<EventObject*> HandleTable::GetEvent(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Event);
    if (!slot_res)
        return slot_res.error();
    return &events_[slots_[*slot_res].entry.object_id];
}

Result<MutexObject*> HandleTable::GetMutex(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Mutex);
    if (!slot_res)
        return slot_res.error();
    return &mutexes_[slots_[*slot_res].entry.object_id];
}

Result<SemaphoreObject*> HandleTable::GetSemaphore(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Semaphore);
    if (!slot_res)
        return slot_res.error();
    return &semaphores_[slots_[*slot_res].entry.object_id];
}

Result<TimerObject*> HandleTable::GetTimer(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Timer);
    if (!slot_res)
        return slot_res.error();
    return &timers_[slots_[*slot_res].entry.object_id];
}

Result<ThreadId> HandleTable::GetThreadId(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Thread);
    if (!slot_res)
        return slot_res.error();
    return slots_[*slot_res].entry.object_id;
}

Result<HandleType> HandleTable::GetHandleType(GuestHandle handle) const {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return slot_res.error();
    return slots_[*slot_res].entry.type;
}

bool HandleTable::IsObjectSignaled(GuestHandle handle, ThreadId tid) const {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return false;
    const auto& s = slots_[*slot_res];
    switch (s.entry.type) {
    case HandleType::Event:
        return events_[s.entry.object_id].is_signaled();
    case HandleType::Mutex:
        return mutexes_[s.entry.object_id].is_signaled(tid);
    case HandleType::Semaphore:
        return semaphores_[s.entry.object_id].is_signaled();
    case HandleType::Timer:
        return timers_[s.entry.object_id].is_signaled();
    case HandleType::Thread:
    case HandleType::None:
    case HandleType::File:
        return false;
    }
    return false;
}

bool HandleTable::SatisfyWait(GuestHandle handle, ThreadId tid) {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return false;
    auto& s = slots_[*slot_res];
    switch (s.entry.type) {
    case HandleType::Event:
        return events_[s.entry.object_id].SatisfyWait(tid);
    case HandleType::Mutex:
        return mutexes_[s.entry.object_id].SatisfyWait(tid);
    case HandleType::Semaphore:
        return semaphores_[s.entry.object_id].SatisfyWait(tid);
    case HandleType::Timer:
        return timers_[s.entry.object_id].SatisfyWait(tid);
    case HandleType::Thread:
    case HandleType::None:
    case HandleType::File:
        return false;
    }
    return false;
}

void HandleTable::AddWaitingThread(GuestHandle handle, ThreadId tid) {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return;
    auto& s = slots_[*slot_res];
    switch (s.entry.type) {
    case HandleType::Event:
        events_[s.entry.object_id].AddWaitingThread(tid);
        break;
    case HandleType::Mutex:
        mutexes_[s.entry.object_id].AddWaitingThread(tid);
        break;
    case HandleType::Semaphore:
        semaphores_[s.entry.object_id].AddWaitingThread(tid);
        break;
    case HandleType::Timer:
        timers_[s.entry.object_id].AddWaitingThread(tid);
        break;
    case HandleType::Thread:
    case HandleType::None:
    case HandleType::File:
        break;
    }
}

void HandleTable::RemoveWaitingThread(GuestHandle handle, ThreadId tid) {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return;
    auto& s = slots_[*slot_res];
    switch (s.entry.type) {
    case HandleType::Event:
        events_[s.entry.object_id].RemoveWaitingThread(tid);
        break;
    case HandleType::Mutex:
        mutexes_[s.entry.object_id].RemoveWaitingThread(tid);
        break;
    case HandleType::Semaphore:
        semaphores_[s.entry.object_id].RemoveWaitingThread(tid);
        break;
    case HandleType::Timer:
        timers_[s.entry.object_id].RemoveWaitingThread(tid);
        break;
    case HandleType::Thread:
    case HandleType::None:
    case HandleType::File:
        break;
    }
}

Result<void> HandleTable::CloseHandle(GuestHandle handle) {
    auto slot_res = ValidateSlotOnly(handle);
    if (!slot_res)
        return slot_res.error();

    std::size_t idx = *slot_res;
    slots_[idx].in_use = false;
    slots_[idx].entry.type = HandleType::None;
    slots_[idx].entry.generation++; // Generational advancement
    return Result<void>::Ok();
}

} // namespace xblob::kernel
