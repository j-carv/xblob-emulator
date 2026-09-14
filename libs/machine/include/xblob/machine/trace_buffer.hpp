#pragma once

#include "xblob/common/types.hpp"

#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace xblob::machine {

enum class TraceEventType : u8 {
    Instruction = 1,
    Exception = 2,
    KernelHle = 3,
    ThreadSwitch = 4,
};

[[nodiscard]] constexpr const char* ToString(TraceEventType type) noexcept {
    switch (type) {
    case TraceEventType::Instruction:
        return "Instruction";
    case TraceEventType::Exception:
        return "Exception";
    case TraceEventType::KernelHle:
        return "KernelHle";
    case TraceEventType::ThreadSwitch:
        return "ThreadSwitch";
    }
    return "Unknown";
}

struct TraceEvent {
    Cycle cycle{0};
    TraceEventType type{TraceEventType::Instruction};
    u32 thread_id{0};
    GuestAddr eip{0};
    u32 data0{0};
    u32 data1{0};
    std::string detail;
};

class TraceRingBuffer {
public:
    explicit TraceRingBuffer(std::size_t capacity = 1024);

    void RecordInstruction(Cycle cycle, u32 thread_id, GuestAddr eip, u32 opcode,
                           std::string_view mnemonic = "");
    void RecordException(Cycle cycle, u32 thread_id, GuestAddr eip, u32 vector, u32 error_code);
    void RecordKernelHle(Cycle cycle, u32 thread_id, GuestAddr eip, u32 ordinal, u32 return_value,
                         std::string_view export_name = "");
    void RecordThreadSwitch(Cycle cycle, u32 from_tid, u32 to_tid, GuestAddr next_eip);

    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] u64 total_recorded() const noexcept;
    [[nodiscard]] u64 dropped_count() const noexcept;

    [[nodiscard]] std::vector<TraceEvent> Snapshot() const;
    [[nodiscard]] std::string FormatText() const;

    void Clear() noexcept;

private:
    std::size_t capacity_;
    std::deque<TraceEvent> events_;
    u64 total_recorded_{0};
    u64 dropped_count_{0};
    mutable std::mutex mutex_;

    void PushEventLocked(TraceEvent event);
};

} // namespace xblob::machine
