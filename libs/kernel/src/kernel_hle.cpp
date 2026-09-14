#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

KernelHle::KernelHle(GuestAddr heap_base, std::size_t heap_size,
                     std::shared_ptr<IDebugSink> debug_sink)
    : heap_manager_(heap_base, heap_size), debug_sink_(std::move(debug_sink)) {
    if (!debug_sink_) {
        debug_sink_ = std::make_shared<BufferedDebugSink>();
    }
    RegisterDomainExports();
}

void KernelHle::Reset() {
    heap_manager_.Reset();
    vm_.Reset();
    handles_.Reset();
    threads_.Reset();
    time_.Reset();
    if (auto* buffered = dynamic_cast<BufferedDebugSink*>(debug_sink_.get())) {
        buffered->Clear();
    }
}

void KernelHle::RegisterDomainExports() {
    RegisterMemoryExports(registry_);
    RegisterTimeExports(registry_);
    RegisterSyncExports(registry_);
    RegisterThreadExports(registry_);
    RegisterIoExports(registry_);
}

Result<u32> KernelHle::DispatchThunk(u32 ordinal, cpu::CpuContext& ctx, memory::AddressSpace& mem) {
    const auto* entry = registry_.FindByOrdinal(ordinal);
    if (!entry) {
        // Collect bounded context for unsupported export diagnostics
        ThreadId tid = threads_.current_thread_id();
        GuestAddr eip = ctx.eip;
        GuestAddr esp = ctx.GetGpr(cpu::Reg32::ESP);

        std::vector<u32> recorded_args;
        for (std::size_t i = 0; i < 8; ++i) {
            GuestAddr arg_addr = esp + static_cast<GuestAddr>((i + 1) * 4);
            auto read_res = mem.Read32(arg_addr);
            if (!read_res) {
                break;
            }
            recorded_args.push_back(*read_res);
        }

        registry_.RecordUnsupportedExport(ordinal, tid, eip, recorded_args);
        return Error{ErrorCode::UnsupportedFeature, "Ordinal de kernel não suportado", ordinal};
    }

    GuestContext gctx{ctx, mem, *this};
    auto call_res = entry->handler(gctx);
    if (!call_res) {
        registry_.RecordDispatchFailure();
        return call_res.error();
    }

    registry_.RecordDispatchSuccess();

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
