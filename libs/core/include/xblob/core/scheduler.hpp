#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_set>
#include <vector>

namespace xblob::core {

using EventCallback = std::function<void(Cycle cycle)>;

enum class SchedulerStatus { Idle, TargetReached, EventLimitReached };

inline std::ostream& operator<<(std::ostream& os, SchedulerStatus status) {
    switch (status) {
    case SchedulerStatus::Idle:
        return os << "Idle";
    case SchedulerStatus::TargetReached:
        return os << "TargetReached";
    case SchedulerStatus::EventLimitReached:
        return os << "EventLimitReached";
    }
    return os << "Unknown";
}

class DeterministicScheduler {
public:
    explicit DeterministicScheduler(Cycle initial_cycle = 0);

    [[nodiscard]] Cycle current_cycle() const noexcept { return current_cycle_; }
    [[nodiscard]] std::size_t pending_event_count() const noexcept { return pending_ids_.size(); }

    [[nodiscard]] Result<void> AdvanceCycles(Cycle delta);

    [[nodiscard]] Result<EventId> ScheduleEvent(Cycle target_cycle, EventCallback callback);
    [[nodiscard]] Result<EventId> ScheduleRelative(Cycle delta_cycles, EventCallback callback);

    [[nodiscard]] Result<void> CancelEvent(EventId id);

    [[nodiscard]] Result<SchedulerStatus>
    RunUntil(Cycle target_cycle, std::size_t max_events = std::numeric_limits<std::size_t>::max());

    [[nodiscard]] Result<SchedulerStatus>
    StepCycles(Cycle delta, std::size_t max_events = std::numeric_limits<std::size_t>::max());

private:
    struct Event {
        EventId id{0};
        Cycle target_cycle{0};
        u64 sequence{0};
        EventCallback callback;

        bool operator>(const Event& other) const noexcept {
            if (target_cycle != other.target_cycle) {
                return target_cycle > other.target_cycle;
            }
            return sequence > other.sequence;
        }
    };

    Cycle current_cycle_{0};
    EventId next_event_id_{1};
    u64 next_sequence_{0};

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue_;
    std::unordered_set<EventId> pending_ids_;
    std::unordered_set<EventId> canceled_ids_;
};

} // namespace xblob::core
