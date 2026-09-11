#pragma once

#include "xblob/common/types.hpp"

#include <optional>
#include <ostream>

namespace xblob::cpu {

enum class ExceptionVector : u8 {
    DivideError = 0,
    Debug = 1,
    Nmi = 2,
    Breakpoint = 3,
    Overflow = 4,
    BoundRangeExceeded = 5,
    InvalidOpcode = 6, // #UD
    DeviceNotAvailable = 7,
    DoubleFault = 8, // #DF
    CoprocessorSegmentOverrun = 9,
    InvalidTss = 10,        // #TS
    SegmentNotPresent = 11, // #NP
    StackSegmentFault = 12, // #SS
    GeneralProtection = 13, // #GP
    PageFault = 14,         // #PF
};

inline const char* ToString(ExceptionVector vec) noexcept {
    switch (vec) {
    case ExceptionVector::DivideError:
        return "#DE (Divide Error)";
    case ExceptionVector::Debug:
        return "#DB (Debug)";
    case ExceptionVector::Nmi:
        return "NMI";
    case ExceptionVector::Breakpoint:
        return "#BP (Breakpoint)";
    case ExceptionVector::Overflow:
        return "#OF (Overflow)";
    case ExceptionVector::BoundRangeExceeded:
        return "#BR (BOUND Range Exceeded)";
    case ExceptionVector::InvalidOpcode:
        return "#UD (Invalid Opcode)";
    case ExceptionVector::DeviceNotAvailable:
        return "#NM (Device Not Available)";
    case ExceptionVector::DoubleFault:
        return "#DF (Double Fault)";
    case ExceptionVector::CoprocessorSegmentOverrun:
        return "#MF (Coprocessor Segment Overrun)";
    case ExceptionVector::InvalidTss:
        return "#TS (Invalid TSS)";
    case ExceptionVector::SegmentNotPresent:
        return "#NP (Segment Not Present)";
    case ExceptionVector::StackSegmentFault:
        return "#SS (Stack Segment Fault)";
    case ExceptionVector::GeneralProtection:
        return "#GP (General Protection Fault)";
    case ExceptionVector::PageFault:
        return "#PF (Page Fault)";
    }
    return "Unknown Exception";
}

inline std::ostream& operator<<(std::ostream& os, ExceptionVector vec) {
    return os << ToString(vec);
}

struct CpuException {
    ExceptionVector vector{ExceptionVector::InvalidOpcode};
    std::optional<u32> error_code{std::nullopt};
    u32 fault_eip{0};
    u32 cr2{0};

    [[nodiscard]] constexpr bool has_error_code() const noexcept { return error_code.has_value(); }
};

} // namespace xblob::cpu
