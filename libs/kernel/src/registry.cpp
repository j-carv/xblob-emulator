#include "xblob/kernel/registry.hpp"

#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

Result<u32> GuestContext::ReadArg(std::size_t index) const {
    // Stdcall arguments on stack:
    // [ESP+0] return address to guest code
    // [ESP+4] argument 0
    // [ESP+8] argument 1 ...
    GuestAddr esp = cpu.GetGpr(cpu::Reg32::ESP);
    GuestAddr arg_addr = esp + static_cast<GuestAddr>((index + 1) * 4);
    return mem.Read32(arg_addr);
}

Result<std::string> GuestContext::ReadString(GuestAddr addr, std::size_t max_len) const {
    std::string str;
    for (std::size_t i = 0; i < max_len; ++i) {
        auto byte_res = mem.Read8(addr + static_cast<GuestAddr>(i));
        if (!byte_res)
            return byte_res.error();
        u8 c = *byte_res;
        if (c == '\0') {
            return str;
        }
        str.push_back(static_cast<char>(c));
    }
    return Error{ErrorCode::LimitReached, "String excede tamanho máximo sem terminador nulo"};
}

Result<u32> GuestContext::Read32(GuestAddr addr) const {
    return mem.Read32(addr);
}

Result<void> GuestContext::Write32(GuestAddr addr, u32 val) const {
    return mem.Write32(addr, val);
}

Result<void> GuestContext::ReadBytes(GuestAddr addr, MutableByteSpan dest) const {
    return mem.ReadBytes(addr, dest);
}

Result<void> GuestContext::WriteBytes(GuestAddr addr, ByteSpan src) const {
    return mem.WriteBytes(addr, src);
}

void GuestContext::SetReturnValue(u32 val) noexcept {
    cpu.SetGpr(cpu::Reg32::EAX, val);
}

Result<void> ExportRegistry::RegisterExport(u32 ordinal, std::string_view name, u32 param_count,
                                            HleHandler handler) {
    if (exports_.find(ordinal) != exports_.end()) {
        return Error{ErrorCode::InvalidArgument, "Ordinal já registrado", ordinal};
    }
    ExportEntry entry;
    entry.ordinal = ordinal;
    entry.name = std::string(name);
    entry.param_count = param_count;
    entry.handler = std::move(handler);
    exports_[ordinal] = std::move(entry);
    return Result<void>::Ok();
}

const ExportEntry* ExportRegistry::FindByOrdinal(u32 ordinal) const noexcept {
    auto it = exports_.find(ordinal);
    if (it != exports_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ExportRegistry::HasOrdinal(u32 ordinal) const noexcept {
    return exports_.find(ordinal) != exports_.end();
}

void ExportRegistry::RecordDispatchSuccess() noexcept {
    total_dispatches_++;
    successful_dispatches_++;
}

void ExportRegistry::RecordDispatchFailure() noexcept {
    total_dispatches_++;
    failed_dispatches_++;
}

void ExportRegistry::RecordUnsupportedExport(u32 ordinal, ThreadId tid, GuestAddr eip,
                                             const std::vector<u32>& args, std::string_view name) {
    total_dispatches_++;
    unsupported_dispatches_++;

    if (unsupported_counts_.size() < kMaxTrackedUnsupportedOrdinals ||
        unsupported_counts_.find(ordinal) != unsupported_counts_.end()) {
        unsupported_counts_[ordinal]++;
    }

    UnsupportedExportInfo info;
    info.ordinal = ordinal;
    info.thread_id = tid;
    info.eip = eip;
    info.call_count = unsupported_counts_[ordinal];
    info.recorded_args = args;
    info.name = std::string(name);

    last_unsupported_ = info;

    if (unsupported_history_.size() >= kMaxUnsupportedHistory) {
        unsupported_history_.pop_front();
    }
    unsupported_history_.push_back(std::move(info));
}

u64 ExportRegistry::GetUnsupportedCallCount(u32 ordinal) const noexcept {
    auto it = unsupported_counts_.find(ordinal);
    if (it != unsupported_counts_.end()) {
        return it->second;
    }
    return 0;
}

RegistryStats ExportRegistry::GetStats() const noexcept {
    RegistryStats stats;
    stats.total_dispatches = total_dispatches_;
    stats.successful_dispatches = successful_dispatches_;
    stats.unsupported_dispatches = unsupported_dispatches_;
    stats.failed_dispatches = failed_dispatches_;
    stats.registered_export_count = exports_.size();
    return stats;
}

void ExportRegistry::ResetStats() noexcept {
    unsupported_counts_.clear();
    unsupported_history_.clear();
    last_unsupported_.reset();
    total_dispatches_ = 0;
    successful_dispatches_ = 0;
    unsupported_dispatches_ = 0;
    failed_dispatches_ = 0;
}

void ExportRegistry::Clear() noexcept {
    exports_.clear();
    ResetStats();
}

} // namespace xblob::kernel
