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
    if (!signaled_)
        return false;
    if (!manual_reset_) {
        signaled_ = false;
    }
    return true;
}

void EventObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void EventObject::RemoveWaitingThread(ThreadId tid) {
    auto it = std::find(waiting_threads_.begin(), waiting_threads_.end(), tid);
    if (it != waiting_threads_.end()) {
        waiting_threads_.erase(it);
    }
}

MutexObject::MutexObject(bool initial_owner, ThreadId owner_tid) {
    if (initial_owner && owner_tid != kInvalidThreadId) {
        owner_tid_ = owner_tid;
        recursion_count_ = 1;
    }
}

Result<bool> MutexObject::Acquire(ThreadId tid) {
    if (recursion_count_ == 0) {
        owner_tid_ = tid;
        recursion_count_ = 1;
        return true;
    }
    if (owner_tid_ == tid) {
        ++recursion_count_;
        return true;
    }
    return false;
}

Result<void> MutexObject::Release(ThreadId tid) {
    if (recursion_count_ == 0 || owner_tid_ != tid) {
        return Error{ErrorCode::InvalidState, "Mutex não pertence à thread chamadora"};
    }
    --recursion_count_;
    if (recursion_count_ == 0) {
        owner_tid_ = kInvalidThreadId;
    }
    return Result<void>::Ok();
}

void MutexObject::AddWaitingThread(ThreadId tid) {
    if (std::find(waiting_threads_.begin(), waiting_threads_.end(), tid) ==
        waiting_threads_.end()) {
        waiting_threads_.push_back(tid);
    }
}

void MutexObject::RemoveWaitingThread(ThreadId tid) {
    auto it = std::find(waiting_threads_.begin(), waiting_threads_.end(), tid);
    if (it != waiting_threads_.end()) {
        waiting_threads_.erase(it);
    }
}

HandleTable::HandleTable() {
    slots_.resize(64);
}

void HandleTable::Reset() {
    for (auto& s : slots_) {
        s.in_use = false;
        s.entry.type = HandleType::None;
        s.entry.object_id = 0;
        ++s.entry.generation;
    }
    events_.clear();
    mutexes_.clear();
}

Result<GuestHandle> HandleTable::AllocateEvent(bool manual_reset, bool initial_state) {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Event;
            u32 obj_id = static_cast<u32>(events_.size());
            events_.emplace_back(manual_reset, initial_state);
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }
    return Error{ErrorCode::LimitReached, "Tabela de handles cheia"};
}

Result<GuestHandle> HandleTable::AllocateMutex(bool initial_owner, ThreadId owner_tid) {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            slots_[i].entry.type = HandleType::Mutex;
            u32 obj_id = static_cast<u32>(mutexes_.size());
            mutexes_.emplace_back(initial_owner, owner_tid);
            slots_[i].entry.object_id = obj_id;
            return EncodeHandle(static_cast<u16>(i), slots_[i].entry.generation);
        }
    }
    return Error{ErrorCode::LimitReached, "Tabela de handles cheia"};
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
    return Error{ErrorCode::LimitReached, "Tabela de handles cheia"};
}

Result<std::size_t> HandleTable::ValidateAndGetSlot(GuestHandle handle,
                                                    HandleType expected_type) const {
    u16 idx = GetHandleIndex(handle);
    u16 gen = GetHandleGeneration(handle);

    if (idx >= slots_.size()) {
        return Error{ErrorCode::InvalidArgument, "Handle inválido: índice fora dos limites",
                     handle};
    }
    const auto& s = slots_[idx];
    if (!s.in_use || s.entry.generation != gen) {
        return Error{ErrorCode::InvalidArgument, "Handle inválido ou expirado", handle};
    }
    if (s.entry.type != expected_type) {
        return Error{ErrorCode::InvalidArgument, "Tipo de handle incompatível"};
    }
    return static_cast<std::size_t>(idx);
}

Result<EventObject*> HandleTable::GetEvent(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Event);
    if (!slot_res)
        return slot_res.error();
    u32 obj_id = slots_[*slot_res].entry.object_id;
    if (obj_id >= events_.size()) {
        return Error{ErrorCode::InternalError, "Objeto de evento corrompido"};
    }
    return &events_[obj_id];
}

Result<MutexObject*> HandleTable::GetMutex(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Mutex);
    if (!slot_res)
        return slot_res.error();
    u32 obj_id = slots_[*slot_res].entry.object_id;
    if (obj_id >= mutexes_.size()) {
        return Error{ErrorCode::InternalError, "Objeto mutex corrompido"};
    }
    return &mutexes_[obj_id];
}

Result<ThreadId> HandleTable::GetThreadId(GuestHandle handle) {
    auto slot_res = ValidateAndGetSlot(handle, HandleType::Thread);
    if (!slot_res)
        return slot_res.error();
    return slots_[*slot_res].entry.object_id;
}

Result<void> HandleTable::CloseHandle(GuestHandle handle) {
    u16 idx = GetHandleIndex(handle);
    u16 gen = GetHandleGeneration(handle);

    if (idx >= slots_.size()) {
        return Error{ErrorCode::InvalidArgument, "Handle inválido"};
    }
    auto& s = slots_[idx];
    if (!s.in_use || s.entry.generation != gen) {
        return Error{ErrorCode::InvalidArgument, "Handle não está aberto"};
    }

    s.in_use = false;
    s.entry.type = HandleType::None;
    s.entry.object_id = 0;
    ++s.entry.generation;
    return Result<void>::Ok();
}

} // namespace xblob::kernel
