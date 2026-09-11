#include "xblob/core/scheduler.hpp"

#include "xblob/common/safe_math.hpp"

namespace xblob::core {

DeterministicScheduler::DeterministicScheduler(Cycle initial_cycle)
    : current_cycle_(initial_cycle) {}

Result<void> DeterministicScheduler::AdvanceCycles(Cycle delta) {
    Cycle new_cycle = 0;
    if (!CheckedAdd(current_cycle_, delta, new_cycle)) {
        return Error{ErrorCode::IntegerOverflow,
                     "Avanço de ciclos causaria overflow do relógio monotônico", current_cycle_};
    }
    current_cycle_ = new_cycle;
    return Result<void>::Ok();
}

Result<EventId> DeterministicScheduler::ScheduleEvent(Cycle target_cycle, EventCallback callback) {
    if (target_cycle < current_cycle_) {
        return Error{ErrorCode::PastCycle, "Ciclo alvo no passado", target_cycle};
    }

    EventId id = next_event_id_++;
    u64 seq = next_sequence_++;

    Event ev{
        .id = id,
        .target_cycle = target_cycle,
        .sequence = seq,
        .callback = std::move(callback),
    };

    pending_ids_.insert(id);
    event_queue_.push(std::move(ev));

    return id;
}

Result<EventId> DeterministicScheduler::ScheduleRelative(Cycle delta_cycles,
                                                         EventCallback callback) {
    Cycle target_cycle = 0;
    if (!CheckedAdd(current_cycle_, delta_cycles, target_cycle)) {
        return Error{ErrorCode::IntegerOverflow, "Agendamento relativo causaria overflow de ciclo",
                     current_cycle_};
    }
    return ScheduleEvent(target_cycle, std::move(callback));
}

Result<void> DeterministicScheduler::CancelEvent(EventId id) {
    if (pending_ids_.find(id) == pending_ids_.end()) {
        return Error{ErrorCode::EventNotFound, "Evento não encontrado ou já executado/cancelado",
                     id};
    }

    pending_ids_.erase(id);
    canceled_ids_.insert(id);
    return Result<void>::Ok();
}

Result<SchedulerStatus> DeterministicScheduler::RunUntil(Cycle target_cycle,
                                                         std::size_t max_events) {
    if (target_cycle < current_cycle_) {
        return Error{ErrorCode::PastCycle, "Ciclo alvo no passado", target_cycle};
    }

    std::size_t events_fired = 0;

    bool stop = false;
    while (!event_queue_.empty() && !stop) {
        const Event& top = event_queue_.top();

        if (canceled_ids_.find(top.id) != canceled_ids_.end()) {
            canceled_ids_.erase(top.id);
            event_queue_.pop();
        } else if (top.target_cycle > target_cycle) {
            stop = true;
        } else {
            if (events_fired >= max_events) {
                return SchedulerStatus::EventLimitReached;
            }

            // Retrieve event
            Event ev = std::move(const_cast<Event&>(top));
            event_queue_.pop();
            pending_ids_.erase(ev.id);

            current_cycle_ = ev.target_cycle;

            if (ev.callback) {
                ev.callback(current_cycle_);
            }
            events_fired++;
        }
    }

    // Check if remaining pending events at current cycle exceed max_events
    while (!event_queue_.empty() &&
           canceled_ids_.find(event_queue_.top().id) != canceled_ids_.end()) {
        canceled_ids_.erase(event_queue_.top().id);
        event_queue_.pop();
    }

    if (!event_queue_.empty() && event_queue_.top().target_cycle <= target_cycle &&
        events_fired >= max_events) {
        return SchedulerStatus::EventLimitReached;
    }

    current_cycle_ = target_cycle;
    return SchedulerStatus::TargetReached;
}

Result<SchedulerStatus> DeterministicScheduler::StepCycles(Cycle delta, std::size_t max_events) {
    Cycle target_cycle = 0;
    if (!CheckedAdd(current_cycle_, delta, target_cycle)) {
        return Error{ErrorCode::IntegerOverflow, "Avanço de ciclos causaria overflow do relógio",
                     current_cycle_};
    }
    return RunUntil(target_cycle, max_events);
}

} // namespace xblob::core
