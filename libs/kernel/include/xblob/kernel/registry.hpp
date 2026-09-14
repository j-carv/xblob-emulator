#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/kernel/types.hpp"
#include "xblob/memory/address_space.hpp"

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xblob::kernel {

class KernelHle;

struct GuestContext {
    cpu::CpuContext& cpu;
    memory::AddressSpace& mem;
    KernelHle& kernel;

    [[nodiscard]] Result<u32> ReadArg(std::size_t index) const;
    [[nodiscard]] Result<std::string> ReadString(GuestAddr addr, std::size_t max_len = 1024) const;
    [[nodiscard]] Result<u32> Read32(GuestAddr addr) const;
    [[nodiscard]] Result<void> Write32(GuestAddr addr, u32 val) const;
    [[nodiscard]] Result<void> ReadBytes(GuestAddr addr, MutableByteSpan dest) const;
    [[nodiscard]] Result<void> WriteBytes(GuestAddr addr, ByteSpan src) const;
    void SetReturnValue(u32 val) noexcept;
};

using HleHandler = std::function<Result<u32>(GuestContext& ctx)>;

struct ExportEntry {
    u32 ordinal{0};
    std::string name;
    u32 param_count{0}; // For stdcall stack cleanup
    HleHandler handler;
};

struct UnsupportedExportInfo {
    u32 ordinal{0};
    ThreadId thread_id{kInvalidThreadId};
    GuestAddr eip{0};
    u64 call_count{0};
    std::vector<u32> recorded_args; // Bounded up to 8 arguments
    std::string name;
};

struct RegistryStats {
    u64 total_dispatches{0};
    u64 successful_dispatches{0};
    u64 unsupported_dispatches{0};
    u64 failed_dispatches{0};
    std::size_t registered_export_count{0};
};

class ExportRegistry {
public:
    static constexpr std::size_t kMaxUnsupportedHistory = 32;
    static constexpr std::size_t kMaxTrackedUnsupportedOrdinals = 64;

    ExportRegistry() = default;

    Result<void> RegisterExport(u32 ordinal, std::string_view name, u32 param_count,
                                HleHandler handler);

    [[nodiscard]] const ExportEntry* FindByOrdinal(u32 ordinal) const noexcept;
    [[nodiscard]] bool HasOrdinal(u32 ordinal) const noexcept;
    [[nodiscard]] std::size_t count() const noexcept { return exports_.size(); }

    void RecordDispatchSuccess() noexcept;
    void RecordDispatchFailure() noexcept;
    void RecordUnsupportedExport(u32 ordinal, ThreadId tid, GuestAddr eip,
                                 const std::vector<u32>& args, std::string_view name = "");

    [[nodiscard]] const std::optional<UnsupportedExportInfo>&
    last_unsupported_export() const noexcept {
        return last_unsupported_;
    }

    [[nodiscard]] const std::deque<UnsupportedExportInfo>& unsupported_history() const noexcept {
        return unsupported_history_;
    }

    [[nodiscard]] u64 GetUnsupportedCallCount(u32 ordinal) const noexcept;
    [[nodiscard]] RegistryStats GetStats() const noexcept;

    void ResetStats() noexcept;
    void Clear() noexcept;

private:
    std::map<u32, ExportEntry> exports_;
    std::map<u32, u64> unsupported_counts_;
    std::deque<UnsupportedExportInfo> unsupported_history_;
    std::optional<UnsupportedExportInfo> last_unsupported_{std::nullopt};
    u64 total_dispatches_{0};
    u64 successful_dispatches_{0};
    u64 unsupported_dispatches_{0};
    u64 failed_dispatches_{0};
};

} // namespace xblob::kernel
