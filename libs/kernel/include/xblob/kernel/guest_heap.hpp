#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/memory/address_space.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace xblob::kernel {

class GuestHeap {
public:
    GuestHeap(GuestAddr base_addr, std::size_t size_bytes, std::size_t alignment = 8);

    [[nodiscard]] Result<GuestAddr> Allocate(std::size_t size_bytes);
    [[nodiscard]] Result<GuestAddr> AllocateAligned(std::size_t size_bytes, std::size_t alignment);
    [[nodiscard]] Result<GuestAddr> ReAllocate(GuestAddr addr, std::size_t new_size,
                                               memory::AddressSpace* mem = nullptr);
    [[nodiscard]] Result<void> Free(GuestAddr addr);
    [[nodiscard]] Result<std::size_t> GetBlockSize(GuestAddr addr) const;

    [[nodiscard]] GuestAddr base_addr() const noexcept { return base_addr_; }
    [[nodiscard]] std::size_t total_size() const noexcept { return total_size_; }
    [[nodiscard]] std::size_t allocated_bytes() const noexcept { return allocated_bytes_; }
    [[nodiscard]] std::size_t free_bytes() const noexcept { return total_size_ - allocated_bytes_; }
    [[nodiscard]] std::size_t active_allocations_count() const noexcept {
        return active_allocations_.size();
    }
    [[nodiscard]] bool IsAllocated(GuestAddr addr) const noexcept {
        return active_allocations_.find(addr) != active_allocations_.end();
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

class GuestHeapManager {
public:
    explicit GuestHeapManager(GuestAddr default_base = 0x10000000,
                              std::size_t default_size = 16 * 1024 * 1024);

    void Reset();

    [[nodiscard]] GuestHeap& default_heap() noexcept { return default_heap_; }
    [[nodiscard]] const GuestHeap& default_heap() const noexcept { return default_heap_; }

    Result<u32> CreateHeap(GuestAddr base, std::size_t size, std::size_t alignment = 8);
    Result<void> DestroyHeap(u32 heap_handle);
    Result<GuestHeap*> GetHeap(u32 heap_handle);

private:
    GuestHeap default_heap_;
    std::map<u32, std::unique_ptr<GuestHeap>> secondary_heaps_;
    u32 next_secondary_base_{0x20000000};
};

} // namespace xblob::kernel
