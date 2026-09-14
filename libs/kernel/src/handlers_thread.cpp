#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

void RegisterThreadExports(ExportRegistry& reg) {
    // Ordinal 12: DbgPrint (1 parameter: const char* string)
    (void)reg.RegisterExport(12, "DbgPrint", 1, [](GuestContext& ctx) -> Result<u32> {
        auto str_addr = ctx.ReadArg(0);
        if (!str_addr)
            return str_addr.error();
        auto msg = ctx.ReadString(*str_addr);
        if (!msg)
            return msg.error();
        ctx.kernel.debug_sink().OutputDebugString(*msg);
        return kStatusSuccess;
    });

    // Ordinal 13: DbgPrompt (3 parameters: PromptPtr, ResponsePtr, MaxLength)
    (void)reg.RegisterExport(13, "DbgPrompt", 3, [](GuestContext& ctx) -> Result<u32> {
        auto prompt_ptr = ctx.ReadArg(0);
        if (prompt_ptr && *prompt_ptr != 0) {
            auto msg = ctx.ReadString(*prompt_ptr);
            if (msg) {
                ctx.kernel.debug_sink().OutputDebugString(*msg);
            }
        }
        return 0;
    });

    // Ordinal 10: CreateThread (6 parameters: Security, StackSize, StartRoutine, Arg, Flags, OutId)
    (void)reg.RegisterExport(10, "CreateThread", 6, [](GuestContext& ctx) -> Result<u32> {
        auto stack_size_arg = ctx.ReadArg(1);
        auto start_routine = ctx.ReadArg(2);
        auto arg = ctx.ReadArg(3);
        auto out_id = ctx.ReadArg(5);
        if (!start_routine || !arg)
            return Error{ErrorCode::InvalidArgument, "Argumentos inválidos para CreateThread"};

        std::size_t stack_size =
            (stack_size_arg && *stack_size_arg >= 16384) ? *stack_size_arg : (64 * 1024);

        // Allocate synthetic stack from kernel heap
        auto stack_res = ctx.kernel.heap().Allocate(stack_size);
        if (!stack_res)
            return kInvalidHandle;

        GuestAddr stack_top = *stack_res + static_cast<GuestAddr>(stack_size);
        auto tid_res = ctx.kernel.threads().CreateThread(*start_routine, stack_top, *arg,
                                                         *stack_res, stack_size, true);
        if (!tid_res)
            return kInvalidHandle;

        if (out_id && *out_id != 0) {
            (void)ctx.Write32(*out_id, *tid_res);
        }

        auto h_res = ctx.kernel.handles().AllocateThreadHandle(*tid_res);
        if (!h_res)
            return kInvalidHandle;
        return *h_res;
    });

    // Ordinal 11: ExitThread (1 parameter: ExitCode)
    (void)reg.RegisterExport(11, "ExitThread", 1, [](GuestContext& ctx) -> Result<u32> {
        auto code = ctx.ReadArg(0);
        u32 exit_code = code ? *code : 0;
        return ctx.kernel.threads().ExitCurrent(exit_code, ctx.cpu).has_value()
                   ? kStatusSuccess
                   : kStatusUnsuccessful;
    });

    // Ordinal 40: KeSuspendThread (1 parameter: ThreadObjectOrHandle)
    (void)reg.RegisterExport(40, "KeSuspendThread", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto tid_res = ctx.kernel.handles().GetThreadId(*h);
        ThreadId tid = tid_res.has_value() ? *tid_res : static_cast<ThreadId>(*h);
        auto prev = ctx.kernel.threads().SuspendThread(tid);
        return prev.has_value() ? *prev : 0;
    });

    // Ordinal 41: KeResumeThread (1 parameter: ThreadObjectOrHandle)
    (void)reg.RegisterExport(41, "KeResumeThread", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto tid_res = ctx.kernel.handles().GetThreadId(*h);
        ThreadId tid = tid_res.has_value() ? *tid_res : static_cast<ThreadId>(*h);
        auto prev = ctx.kernel.threads().ResumeThread(tid);
        return prev.has_value() ? *prev : 0;
    });

    // Ordinal 42: KeGetCurrentThread (0 parameters)
    (void)reg.RegisterExport(42, "KeGetCurrentThread", 0, [](GuestContext& ctx) -> Result<u32> {
        return ctx.kernel.threads().current_thread_id();
    });

    // Ordinal 55: KeYieldExecution (0 parameters)
    (void)reg.RegisterExport(55, "KeYieldExecution", 0, [](GuestContext& ctx) -> Result<u32> {
        (void)ctx.kernel.threads().Yield(ctx.cpu);
        return 1;
    });

    // Ordinal 212: NtTerminateThread (2 parameters: Handle, ExitCode)
    (void)reg.RegisterExport(212, "NtTerminateThread", 2, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto code = ctx.ReadArg(1);
        u32 exit_code = code ? *code : 0;
        if (!h || *h == 0 || *h == 0xFFFFFFFFU) {
            return ctx.kernel.threads().ExitCurrent(exit_code, ctx.cpu).has_value()
                       ? kStatusSuccess
                       : kStatusUnsuccessful;
        }

        auto tid_res = ctx.kernel.handles().GetThreadId(*h);
        ThreadId tid = tid_res.has_value() ? *tid_res : static_cast<ThreadId>(*h);
        return ctx.kernel.threads().TerminateThread(tid, exit_code).has_value()
                   ? kStatusSuccess
                   : kStatusUnsuccessful;
    });
}

} // namespace xblob::kernel
