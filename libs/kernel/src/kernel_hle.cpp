#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

KernelHle::KernelHle(GuestAddr heap_base, std::size_t heap_size,
                     std::shared_ptr<IDebugSink> debug_sink)
    : heap_(heap_base, heap_size), debug_sink_(std::move(debug_sink)) {
    if (!debug_sink_) {
        debug_sink_ = std::make_shared<BufferedDebugSink>();
    }
    RegisterStandardExports();
}

void KernelHle::Reset() {
    heap_.Reset();
    handles_.Reset();
    threads_.Reset();
    if (auto* buffered = dynamic_cast<BufferedDebugSink*>(debug_sink_.get())) {
        buffered->Clear();
    }
}

void KernelHle::RegisterStandardExports() {
    // Ordinal 12: DbgPrint (1 parameter: const char* string)
    (void)registry_.RegisterExport(12, "DbgPrint", 1, [](GuestContext& ctx) -> Result<u32> {
        auto str_addr = ctx.ReadArg(0);
        if (!str_addr)
            return str_addr.error();
        auto msg = ctx.ReadString(*str_addr);
        if (!msg)
            return msg.error();
        ctx.kernel.debug_sink().OutputDebugString(*msg);
        return kStatusSuccess;
    });

    // Ordinal 184: RtlAllocateHeap (3 parameters: HeapHandle, Flags, Size)
    (void)registry_.RegisterExport(184, "RtlAllocateHeap", 3, [](GuestContext& ctx) -> Result<u32> {
        auto size = ctx.ReadArg(2);
        if (!size)
            return size.error();
        auto alloc_res = ctx.kernel.heap().Allocate(*size);
        if (!alloc_res)
            return 0; // NULL on failure
        return *alloc_res;
    });

    // Ordinal 185: RtlFreeHeap (3 parameters: HeapHandle, Flags, BaseAddress)
    (void)registry_.RegisterExport(185, "RtlFreeHeap", 3, [](GuestContext& ctx) -> Result<u32> {
        auto ptr = ctx.ReadArg(2);
        if (!ptr)
            return ptr.error();
        auto free_res = ctx.kernel.heap().Free(*ptr);
        return free_res.has_value() ? 1 : 0;
    });

    // Ordinal 10: CreateThread (6 parameters: Security, StackSize, StartRoutine, Arg, Flags, OutId)
    (void)registry_.RegisterExport(10, "CreateThread", 6, [](GuestContext& ctx) -> Result<u32> {
        auto start_routine = ctx.ReadArg(2);
        auto arg = ctx.ReadArg(3);
        if (!start_routine || !arg)
            return Error{ErrorCode::InvalidArgument, "Argumentos inválidos para CreateThread"};

        // Allocate a synthetic stack from the heap
        constexpr std::size_t kDefaultStackSize = 64 * 1024;
        auto stack_res = ctx.kernel.heap().Allocate(kDefaultStackSize);
        if (!stack_res)
            return kInvalidHandle;

        GuestAddr stack_top = *stack_res + static_cast<GuestAddr>(kDefaultStackSize);
        auto tid_res = ctx.kernel.threads().CreateThread(*start_routine, stack_top, *arg,
                                                         *stack_res, kDefaultStackSize);
        if (!tid_res)
            return kInvalidHandle;

        auto h_res = ctx.kernel.handles().AllocateThreadHandle(*tid_res);
        if (!h_res)
            return kInvalidHandle;
        return *h_res;
    });

    // Ordinal 11: ExitThread (1 parameter: ExitCode)
    (void)registry_.RegisterExport(11, "ExitThread", 1, [](GuestContext& ctx) -> Result<u32> {
        auto code = ctx.ReadArg(0);
        u32 exit_code = code ? *code : 0;
        return ctx.kernel.threads().ExitCurrent(exit_code, ctx.cpu).has_value()
                   ? kStatusSuccess
                   : kStatusUnsuccessful;
    });

    // Ordinal 64: CreateEvent (4 parameters: Security, ManualReset, InitialState, Name)
    (void)registry_.RegisterExport(64, "CreateEvent", 4, [](GuestContext& ctx) -> Result<u32> {
        auto manual = ctx.ReadArg(1);
        auto initial = ctx.ReadArg(2);
        bool m = manual ? (*manual != 0) : false;
        bool init = initial ? (*initial != 0) : false;
        auto handle_res = ctx.kernel.handles().AllocateEvent(m, init);
        if (!handle_res)
            return kInvalidHandle;
        return *handle_res;
    });

    // Ordinal 65: SetEvent (1 parameter: EventHandle)
    (void)registry_.RegisterExport(65, "SetEvent", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto ev = ctx.kernel.handles().GetEvent(*h);
        if (!ev)
            return 0;
        (*ev)->Set();

        // Wake any waiting threads
        for (ThreadId tid : (*ev)->waiting_threads()) {
            (void)ctx.kernel.threads().Wake(tid);
        }
        return 1;
    });

    // Ordinal 66: ResetEvent (1 parameter: EventHandle)
    (void)registry_.RegisterExport(66, "ResetEvent", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto ev = ctx.kernel.handles().GetEvent(*h);
        if (!ev)
            return 0;
        (*ev)->Reset();
        return 1;
    });

    // Ordinal 80: CreateMutex (3 parameters: Security, InitialOwner, Name)
    (void)registry_.RegisterExport(80, "CreateMutex", 3, [](GuestContext& ctx) -> Result<u32> {
        auto initial_owner = ctx.ReadArg(1);
        bool init = initial_owner ? (*initial_owner != 0) : false;
        ThreadId owner_tid = init ? ctx.kernel.threads().current_thread_id() : kInvalidThreadId;
        auto h_res = ctx.kernel.handles().AllocateMutex(init, owner_tid);
        if (!h_res)
            return kInvalidHandle;
        return *h_res;
    });

    // Ordinal 84: ReleaseMutex (1 parameter: MutexHandle)
    (void)registry_.RegisterExport(84, "ReleaseMutex", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return 0;
        auto m = ctx.kernel.handles().GetMutex(*h);
        if (!m)
            return 0;
        auto rel_res = (*m)->Release(ctx.kernel.threads().current_thread_id());
        if (!rel_res)
            return 0;

        for (ThreadId tid : (*m)->waiting_threads()) {
            (void)ctx.kernel.threads().Wake(tid);
        }
        return 1;
    });

    // Ordinal 90: WaitForSingleObject (2 parameters: Handle, TimeoutMs)
    (void)registry_.RegisterExport(
        90, "WaitForSingleObject", 2, [](GuestContext& ctx) -> Result<u32> {
            auto h = ctx.ReadArg(0);
            if (!h)
                return kStatusInvalidHandle;

            // Check if it is an event
            auto ev_res = ctx.kernel.handles().GetEvent(*h);
            if (ev_res.has_value()) {
                auto* ev = *ev_res;
                if (ev->SatisfyWait(ctx.kernel.threads().current_thread_id())) {
                    return kStatusSuccess;
                }
                ev->AddWaitingThread(ctx.kernel.threads().current_thread_id());
                (void)ctx.kernel.threads().BlockCurrent(*h, ctx.cpu);
                return kStatusSuccess;
            }

            // Check if it is a mutex
            auto m_res = ctx.kernel.handles().GetMutex(*h);
            if (m_res.has_value()) {
                auto* m = *m_res;
                auto acq = m->Acquire(ctx.kernel.threads().current_thread_id());
                if (acq.has_value() && *acq) {
                    return kStatusSuccess;
                }
                m->AddWaitingThread(ctx.kernel.threads().current_thread_id());
                (void)ctx.kernel.threads().BlockCurrent(*h, ctx.cpu);
                return kStatusSuccess;
            }

            return kStatusInvalidHandle;
        });
}

Result<u32> KernelHle::DispatchThunk(u32 ordinal, cpu::CpuContext& ctx, memory::AddressSpace& mem) {
    const auto* entry = registry_.FindByOrdinal(ordinal);
    if (!entry) {
        return Error{ErrorCode::UnsupportedFeature, "Ordinal de kernel não suportado", ordinal};
    }

    GuestContext gctx{ctx, mem, *this};
    auto call_res = entry->handler(gctx);
    if (!call_res) {
        return call_res.error();
    }

    u32 return_val = *call_res;
    gctx.SetReturnValue(return_val);

    // Stdcall return stack adjustment:
    // Read return address at [ESP]
    GuestAddr esp = ctx.GetGpr(cpu::Reg32::ESP);
    auto ret_addr_res = mem.Read32(esp);
    if (!ret_addr_res) {
        return ret_addr_res.error();
    }

    // Advance ESP past return address and parameters
    ctx.SetGpr(cpu::Reg32::ESP, esp + 4 + (entry->param_count * 4));
    ctx.eip = *ret_addr_res;

    return return_val;
}

} // namespace xblob::kernel
