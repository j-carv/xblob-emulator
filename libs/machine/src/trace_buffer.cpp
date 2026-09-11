#include "xblob/machine/trace_buffer.hpp"

#include <iomanip>
#include <sstream>

namespace xblob::machine {

TraceRingBuffer::TraceRingBuffer(std::size_t capacity) : capacity_(capacity ? capacity : 1024) {}

void TraceRingBuffer::PushEvent(TraceEvent event) {
    ++total_recorded_;
    if (events_.size() >= capacity_) {
        events_.pop_front();
        ++dropped_count_;
    }
    events_.push_back(std::move(event));
}

void TraceRingBuffer::RecordInstruction(Cycle cycle, u32 thread_id, GuestAddr eip, u32 opcode,
                                        std::string_view mnemonic) {
    PushEvent(TraceEvent{
        .cycle = cycle,
        .type = TraceEventType::Instruction,
        .thread_id = thread_id,
        .eip = eip,
        .data0 = opcode,
        .data1 = 0,
        .detail = std::string(mnemonic),
    });
}

void TraceRingBuffer::RecordException(Cycle cycle, u32 thread_id, GuestAddr eip, u32 vector,
                                      u32 error_code) {
    PushEvent(TraceEvent{
        .cycle = cycle,
        .type = TraceEventType::Exception,
        .thread_id = thread_id,
        .eip = eip,
        .data0 = vector,
        .data1 = error_code,
        .detail = "",
    });
}

void TraceRingBuffer::RecordKernelHle(Cycle cycle, u32 thread_id, GuestAddr eip, u32 ordinal,
                                      u32 return_value, std::string_view export_name) {
    PushEvent(TraceEvent{
        .cycle = cycle,
        .type = TraceEventType::KernelHle,
        .thread_id = thread_id,
        .eip = eip,
        .data0 = ordinal,
        .data1 = return_value,
        .detail = std::string(export_name),
    });
}

void TraceRingBuffer::RecordThreadSwitch(Cycle cycle, u32 from_tid, u32 to_tid,
                                         GuestAddr next_eip) {
    PushEvent(TraceEvent{
        .cycle = cycle,
        .type = TraceEventType::ThreadSwitch,
        .thread_id = to_tid,
        .eip = next_eip,
        .data0 = from_tid,
        .data1 = to_tid,
        .detail = "",
    });
}

std::vector<TraceEvent> TraceRingBuffer::Snapshot() const {
    return {events_.begin(), events_.end()};
}

std::string TraceRingBuffer::FormatText() const {
    std::ostringstream ss;
    for (const auto& ev : events_) {
        ss << "[" << std::dec << ev.cycle << "] ";
        ss << "TID=" << ev.thread_id << " ";
        ss << "EIP=0x" << std::hex << std::setw(8) << std::setfill('0') << ev.eip << " ";
        ss << ToString(ev.type);

        switch (ev.type) {
        case TraceEventType::Instruction:
            ss << " op=0x" << std::hex << ev.data0;
            if (!ev.detail.empty()) {
                ss << " (" << ev.detail << ")";
            }
            break;
        case TraceEventType::Exception:
            ss << " vec=" << std::dec << ev.data0 << " err=0x" << std::hex << ev.data1;
            break;
        case TraceEventType::KernelHle:
            ss << " ord=" << std::dec << ev.data0 << " ret=0x" << std::hex << ev.data1;
            if (!ev.detail.empty()) {
                ss << " (" << ev.detail << ")";
            }
            break;
        case TraceEventType::ThreadSwitch:
            ss << " from=" << std::dec << ev.data0 << " to=" << ev.data1;
            break;
        }
        ss << "\n";
    }
    return ss.str();
}

void TraceRingBuffer::Clear() noexcept {
    events_.clear();
    total_recorded_ = 0;
    dropped_count_ = 0;
}

} // namespace xblob::machine
