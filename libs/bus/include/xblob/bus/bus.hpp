#pragma once

#include "xblob/bus/bus_types.hpp"
#include "xblob/bus/device.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <memory>
#include <vector>

namespace xblob::bus {

struct BusMapping {
    GuestAddr base{0};
    GuestSize size{0};
    std::shared_ptr<BusDevice> device{nullptr};

    [[nodiscard]] constexpr u64 end() const noexcept {
        return static_cast<u64>(base) + static_cast<u64>(size);
    }

    [[nodiscard]] constexpr bool Contains(GuestAddr addr, BusAccessWidth width) const noexcept {
        const u64 req_start = addr;
        const u64 req_end = static_cast<u64>(addr) + static_cast<u64>(width);
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

class Bus {
public:
    Bus() = default;

    [[nodiscard]] Result<void> MapDevice(GuestAddr base, GuestSize size,
                                         std::shared_ptr<BusDevice> device);

    [[nodiscard]] Result<void> UnmapDevice(GuestAddr base);

    [[nodiscard]] Result<u32> Read(GuestAddr addr, BusAccessWidth width);
    [[nodiscard]] Result<void> Write(GuestAddr addr, BusAccessWidth width, u32 value);

    [[nodiscard]] std::size_t mapping_count() const noexcept { return mappings_.size(); }
    [[nodiscard]] const std::vector<BusMapping>& mappings() const noexcept { return mappings_; }

private:
    std::vector<BusMapping> mappings_;
};

} // namespace xblob::bus
