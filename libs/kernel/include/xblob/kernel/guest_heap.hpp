#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace xblob::kernel {

class GuestHeap {
public:
    GuestHeap(GuestAddr base_addr, std::size_t size_bytes, std::size_t alignment = 8);

    [[nodiscard]] Result<GuestAddr> Allocate(std::size_t size_bytes);
    [[nodiscard]] Result<void> Free(GuestAddr addr);

    [[nodiscard]] GuestAddr base_addr() const noexcept { return base_addr_; }
    [[nodiscard]] std::size_t total_size() const noexcept { return total_size_; }
    [[nodiscard]] std::size_t allocated_bytes() const noexcept { return allocated_bytes_; }
    [[nodiscard]] std::size_t free_bytes() const noexcept { return total_size_ - allocated_bytes_; }
    [[nodiscard]] std::size_t active_allocations_count() const noexcept {
        return active_allocations_.size();
    }

    void Reset();

private:
    struct FreeBlock {
        GuestAddr addr{0};
        std::size_t size{0};
    };

    GuestAddr base_addr_;
    std::size_t total_size_;
    std::size_t alignment_;
    std::size_t allocated_bytes_{0};

    // Free blocks sorted by address
    std::vector<FreeBlock> free_blocks_;

    // Active allocations: map from start address to allocated size
    std::map<GuestAddr, std::size_t> active_allocations_;

    void Coalesce();
};

} // namespace xblob::kernel
