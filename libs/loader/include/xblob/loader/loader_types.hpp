#pragma once

#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/memory/permission.hpp"

#include <string>
#include <vector>

namespace xblob::loader {

struct SectionPlan {
    std::string name;
    GuestAddr virtual_address{0};
    GuestSize virtual_size{0};
    u32 raw_address{0};
    GuestSize raw_size{0};
    u32 flags{0};
    memory::MemoryPermission permissions{memory::MemoryPermission::Read};
    GuestSize zero_fill_size{0};

    [[nodiscard]] constexpr u64 virtual_end() const noexcept {
        return static_cast<u64>(virtual_address) + static_cast<u64>(virtual_size);
    }
};

struct ImportThunkPlan {
    u32 ordinal{0};
    GuestAddr thunk_address{0};
    std::string name;
};

struct XbeLoadPlan {
    GuestAddr base_address{0};
    GuestSize headers_size{0};
    GuestSize image_size{0};
    GuestAddr entry_point{0};
    memory::MemoryPermission headers_permissions{memory::MemoryPermission::Read};
    std::vector<SectionPlan> sections;

    bool is_diagnostic_eligible{false};
    std::string diagnostic_eligibility_reason;
    std::vector<ImportThunkPlan> import_thunks;

    [[nodiscard]] constexpr u64 image_end() const noexcept {
        return static_cast<u64>(base_address) + static_cast<u64>(image_size);
    }
};

struct InitialContext {
    GuestAddr entry_point{0};
    GuestAddr base_address{0};
    GuestSize image_size{0};
    GuestAddr stack_top{0};
    GuestSize stack_size{0};
    cpu::CpuContext cpu_context{};
};

} // namespace xblob::loader
