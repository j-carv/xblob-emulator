#include "xblob/kernel/guest_heap.hpp"

#include <algorithm>

namespace xblob::kernel {

GuestHeap::GuestHeap(GuestAddr base_addr, std::size_t size_bytes, std::size_t alignment)
    : base_addr_(base_addr), total_size_(size_bytes), alignment_(alignment ? alignment : 8) {
    Reset();
}

void GuestHeap::Reset() {
    free_blocks_.clear();
    active_allocations_.clear();
    allocated_bytes_ = 0;
    if (total_size_ > 0) {
        free_blocks_.push_back(FreeBlock{base_addr_, total_size_});
    }
}

Result<GuestAddr> GuestHeap::Allocate(std::size_t size_bytes) {
    if (size_bytes == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de alocação não pode ser zero"};
    }

    // Align size up to alignment_
    std::size_t aligned_size = (size_bytes + (alignment_ - 1)) & ~(alignment_ - 1);

    // First-fit deterministic allocation
    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        if (it->size >= aligned_size) {
            GuestAddr alloc_addr = it->addr;
            std::size_t remaining = it->size - aligned_size;

            if (remaining >= alignment_) {
                it->addr += aligned_size;
                it->size = remaining;
            } else {
                aligned_size = it->size;
                free_blocks_.erase(it);
            }

            active_allocations_[alloc_addr] = aligned_size;
            allocated_bytes_ += aligned_size;
            return alloc_addr;
        }
    }

    return Error{ErrorCode::LimitReached, "Memória do heap esgotada"};
}

Result<void> GuestHeap::Free(GuestAddr addr) {
    if (addr < base_addr_ || addr >= base_addr_ + total_size_) {
        return Error{ErrorCode::OutOfBounds, "Endereço fora dos limites do heap", addr};
    }

    auto it = active_allocations_.find(addr);
    if (it == active_allocations_.end()) {
        return Error{ErrorCode::InvalidArgument,
                     "Endereço não alocado ou já liberado (double-free)", addr};
    }

    std::size_t size = it->second;
    active_allocations_.erase(it);
    allocated_bytes_ -= size;

    // Insert back into free blocks sorted by address
    FreeBlock fb{addr, size};
    auto pos =
        std::lower_bound(free_blocks_.begin(), free_blocks_.end(), fb,
                         [](const FreeBlock& a, const FreeBlock& b) { return a.addr < b.addr; });
    free_blocks_.insert(pos, fb);

    Coalesce();
    return Result<void>::Ok();
}

void GuestHeap::Coalesce() {
    if (free_blocks_.size() <= 1)
        return;

    std::vector<FreeBlock> merged;
    merged.reserve(free_blocks_.size());
    merged.push_back(free_blocks_[0]);

    for (std::size_t i = 1; i < free_blocks_.size(); ++i) {
        auto& last = merged.back();
        const auto& curr = free_blocks_[i];
        if (last.addr + last.size == curr.addr) {
            last.size += curr.size;
        } else {
            merged.push_back(curr);
        }
    }

    free_blocks_ = std::move(merged);
}

} // namespace xblob::kernel
