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

void ExportRegistry::Clear() noexcept {
    exports_.clear();
}

} // namespace xblob::kernel
