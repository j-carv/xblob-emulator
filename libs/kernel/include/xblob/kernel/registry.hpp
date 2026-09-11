#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/memory/address_space.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace xblob::kernel {

class KernelHle;

struct GuestContext {
    cpu::CpuContext& cpu;
    memory::AddressSpace& mem;
    KernelHle& kernel;

    [[nodiscard]] Result<u32> ReadArg(std::size_t index) const;
    [[nodiscard]] Result<std::string> ReadString(GuestAddr addr, std::size_t max_len = 1024) const;
    void SetReturnValue(u32 val) noexcept;
};

using HleHandler = std::function<Result<u32>(GuestContext& ctx)>;

struct ExportEntry {
    u32 ordinal{0};
    std::string name;
    u32 param_count{0}; // For stdcall stack cleanup
    HleHandler handler;
};

class ExportRegistry {
public:
    ExportRegistry() = default;

    Result<void> RegisterExport(u32 ordinal, std::string_view name, u32 param_count,
                                HleHandler handler);

    [[nodiscard]] const ExportEntry* FindByOrdinal(u32 ordinal) const noexcept;
    [[nodiscard]] bool HasOrdinal(u32 ordinal) const noexcept;
    [[nodiscard]] std::size_t count() const noexcept { return exports_.size(); }

    void Clear() noexcept;

private:
    std::map<u32, ExportEntry> exports_;
};

} // namespace xblob::kernel
