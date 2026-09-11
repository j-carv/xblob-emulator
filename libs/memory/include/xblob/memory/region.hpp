#pragma once

#include "xblob/common/types.hpp"
#include "xblob/memory/mmio.hpp"
#include "xblob/memory/permission.hpp"
#include "xblob/memory/ram.hpp"

#include <optional>

namespace xblob::memory {

enum class RegionType : u8 { Ram, Mmio };

struct Region {
    GuestAddr base{0};
    GuestSize size{0};
    MemoryPermission permissions{MemoryPermission::None};
    RegionType type{RegionType::Ram};

    // RAM backing
    Ram* ram{nullptr};
    std::size_t ram_offset{0};

    // MMIO backing
    std::optional<MmioHandler> mmio_handler{std::nullopt};

    [[nodiscard]] constexpr u64 end() const noexcept {
        return static_cast<u64>(base) + static_cast<u64>(size);
    }

    [[nodiscard]] constexpr bool ContainsRange(GuestAddr addr, GuestSize len) const noexcept {
        const u64 req_start = addr;
        const u64 req_end = static_cast<u64>(addr) + static_cast<u64>(len);
        return req_start >= base && req_end <= end();
    }

    [[nodiscard]] constexpr bool Overlaps(GuestAddr other_base,
                                          GuestSize other_size) const noexcept {
        const u64 a_start = base;
        const u64 a_end = end();
        const u64 b_start = other_base;
        const u64 b_end = static_cast<u64>(other_base) + static_cast<u64>(other_size);
        return a_start < b_end && b_start < a_end;
    }
};

} // namespace xblob::memory
