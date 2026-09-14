#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

void RegisterTimeExports(ExportRegistry& reg) {
    // Ordinal 108: KeQueryPerformanceCounter (0 parameters, returns low 32 in EAX, high 32 in EDX)
    (void)reg.RegisterExport(108, "KeQueryPerformanceCounter", 0,
                             [](GuestContext& ctx) -> Result<u32> {
                                 u64 count = ctx.kernel.time().QueryPerformanceCounter();
                                 ctx.cpu.SetGpr(cpu::Reg32::EDX, static_cast<u32>(count >> 32));
                                 return static_cast<u32>(count & 0xFFFFFFFFU);
                             });

    // Ordinal 109: KeQueryPerformanceFrequency (0 parameters)
    (void)reg.RegisterExport(109, "KeQueryPerformanceFrequency", 0,
                             [](GuestContext& /*ctx*/) -> Result<u32> {
                                 return static_cast<u32>(kKernelPerformanceCounterFreq);
                             });

    // Ordinal 110: KeQuerySystemTime (1 parameter: CurrentTimePtr)
    (void)reg.RegisterExport(110, "KeQuerySystemTime", 1, [](GuestContext& ctx) -> Result<u32> {
        auto time_ptr = ctx.ReadArg(0);
        if (!time_ptr || *time_ptr == 0) {
            return kStatusInvalidParameter;
        }

        u64 sys_time = ctx.kernel.time().QuerySystemTime();
        auto res_low = ctx.Write32(*time_ptr, static_cast<u32>(sys_time & 0xFFFFFFFFU));
        auto res_high = ctx.Write32(*time_ptr + 4, static_cast<u32>(sys_time >> 32));
        if (!res_low || !res_high) {
            return kStatusAccessViolation;
        }
        return kStatusSuccess;
    });

    // Ordinal 111: KeQueryInterruptTime (0 parameters)
    (void)reg.RegisterExport(111, "KeQueryInterruptTime", 0, [](GuestContext& ctx) -> Result<u32> {
        u64 count = ctx.kernel.time().QueryPerformanceCounter();
        ctx.cpu.SetGpr(cpu::Reg32::EDX, static_cast<u32>(count >> 32));
        return static_cast<u32>(count & 0xFFFFFFFFU);
    });

    // Ordinal 144: KeTickCount (0 parameters)
    (void)reg.RegisterExport(144, "KeTickCount", 0, [](GuestContext& ctx) -> Result<u32> {
        return ctx.kernel.time().QueryTickCount();
    });

    // Ordinal 136: KeSetTimer (4 parameters: TimerHandle, DueTimePtr, PeriodMs, DpcPtr)
    (void)reg.RegisterExport(136, "KeSetTimer", 4, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto due_time_ptr = ctx.ReadArg(1);
        auto period_ms = ctx.ReadArg(2);
        auto dpc = ctx.ReadArg(3);
        if (!h || !due_time_ptr) {
            return 0;
        }

        auto timer_res = ctx.kernel.handles().GetTimer(*h);
        if (!timer_res) {
            return 0;
        }

        auto low_res = ctx.Read32(*due_time_ptr);
        auto high_res = ctx.Read32(*due_time_ptr + 4);
        if (!low_res || !high_res) {
            return 0;
        }

        i64 due_val =
            static_cast<i64>((static_cast<u64>(*high_res) << 32) | static_cast<u64>(*low_res));

        Cycle current_cyc = ctx.kernel.time().current_cycle();
        Cycle delta_cyc = KernelTime::Relative100NsToCycles(due_val);
        Cycle due_cyc = current_cyc + delta_cyc;

        u32 period = period_ms ? *period_ms : 0;
        GuestAddr dpc_addr = dpc ? *dpc : 0;

        bool was_active = (*timer_res)->is_active();
        (*timer_res)->Set(due_cyc, period, dpc_addr, 0);
        return was_active ? 1 : 0;
    });

    // Ordinal 95: KeCancelTimer (1 parameter: TimerHandle)
    (void)reg.RegisterExport(95, "KeCancelTimer", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h) {
            return 0;
        }
        auto timer_res = ctx.kernel.handles().GetTimer(*h);
        if (!timer_res) {
            return 0;
        }
        return (*timer_res)->Cancel() ? 1 : 0;
    });

    // Ordinal 194: NtDelayExecution (2 parameters: Alertable, IntervalPtr)
    (void)reg.RegisterExport(194, "NtDelayExecution", 2, [](GuestContext& ctx) -> Result<u32> {
        auto interval_ptr = ctx.ReadArg(1);
        if (!interval_ptr || *interval_ptr == 0) {
            return kStatusInvalidParameter;
        }

        auto low_res = ctx.Read32(*interval_ptr);
        auto high_res = ctx.Read32(*interval_ptr + 4);
        if (!low_res || !high_res) {
            return kStatusAccessViolation;
        }

        i64 interval_val =
            static_cast<i64>((static_cast<u64>(*high_res) << 32) | static_cast<u64>(*low_res));

        Cycle delay_cycles = KernelTime::Relative100NsToCycles(interval_val);
        Cycle deadline = ctx.kernel.time().current_cycle() + delay_cycles;

        // Block current thread with timeout deadline
        (void)ctx.kernel.threads().BlockCurrentWithTimeout({}, false, true, deadline, ctx.cpu);
        return kStatusSuccess;
    });
}

} // namespace xblob::kernel
