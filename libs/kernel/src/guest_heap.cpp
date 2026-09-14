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

Result<GuestAddr> GuestHeap::AllocateAligned(std::size_t size_bytes, std::size_t alignment) {
    if (size_bytes == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de alocação não pode ser zero"};
    }
    if (alignment < 4 || (alignment & (alignment - 1)) != 0) {
        return Error{ErrorCode::InvalidArgument, "Alinhamento deve ser potência de 2 e >= 4"};
    }

    std::size_t effective_align = std::max(alignment_, alignment);
    std::size_t aligned_size = (size_bytes + (effective_align - 1)) & ~(effective_align - 1);

    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        GuestAddr candidate =
            static_cast<GuestAddr>((it->addr + (effective_align - 1)) & ~(effective_align - 1));
        std::size_t padding = candidate - it->addr;
        if (it->size >= padding + aligned_size) {
            std::size_t total_consumed = padding + aligned_size;
            std::size_t remaining = it->size - total_consumed;

            if (padding > 0) {
                // Keep the prefix before candidate
                it->size = padding;
                if (remaining >= alignment_) {
                    FreeBlock suffix{candidate + static_cast<GuestAddr>(aligned_size), remaining};
                    free_blocks_.insert(it + 1, suffix);
                }
            } else {
                if (remaining >= alignment_) {
                    it->addr += aligned_size;
                    it->size = remaining;
                } else {
                    aligned_size = it->size;
                    free_blocks_.erase(it);
                }
            }

            active_allocations_[candidate] = aligned_size;
            allocated_bytes_ += aligned_size;
            return candidate;
        }
    }

    return Error{ErrorCode::LimitReached, "Memória do heap esgotada para alinhamento"};
}

Result<GuestAddr> GuestHeap::ReAllocate(GuestAddr addr, std::size_t new_size,
                                        memory::AddressSpace* mem) {
    auto it = active_allocations_.find(addr);
    if (it == active_allocations_.end()) {
        return Error{ErrorCode::InvalidArgument, "Endereço não alocado para realocação"};
    }

    std::size_t old_size = it->second;
    if (new_size == 0) {
        auto free_res = Free(addr);
        if (!free_res)
            return free_res.error();
        return 0;
    }

    auto new_addr_res = Allocate(new_size);
    if (!new_addr_res) {
        return new_addr_res.error();
    }

    GuestAddr new_addr = *new_addr_res;
    if (mem != nullptr && new_addr != addr) {
        std::size_t copy_size = std::min(old_size, new_size);
        std::vector<u8> buffer(copy_size);
        auto read_res = mem->ReadBytes(addr, buffer);
        if (read_res.has_value()) {
            (void)mem->WriteBytes(new_addr, buffer);
        }
    }

    (void)Free(addr);
    return new_addr;
}

Result<std::size_t> GuestHeap::GetBlockSize(GuestAddr addr) const {
    auto it = active_allocations_.find(addr);
    if (it == active_allocations_.end()) {
        return Error{ErrorCode::InvalidArgument, "Endereço não alocado"};
    }
    return it->second;
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

GuestHeapManager::GuestHeapManager(GuestAddr default_base, std::size_t default_size)
    : default_heap_(default_base, default_size, 8) {}

void GuestHeapManager::Reset() {
    default_heap_.Reset();
    secondary_heaps_.clear();
    next_secondary_base_ = 0x20000000;
}

Result<u32> GuestHeapManager::CreateHeap(GuestAddr base, std::size_t size, std::size_t alignment) {
    GuestAddr target_base = base;
    if (target_base == 0) {
        target_base = next_secondary_base_;
        next_secondary_base_ += static_cast<u32>((size + 0xFFFFULL) & ~0xFFFFULL);
    }
    auto heap = std::make_unique<GuestHeap>(target_base, size, alignment);
    u32 handle = static_cast<u32>(target_base);
    secondary_heaps_[handle] = std::move(heap);
    return handle;
}

Result<void> GuestHeapManager::DestroyHeap(u32 heap_handle) {
    if (heap_handle == default_heap_.base_addr()) {
        return Error{ErrorCode::InvalidArgument, "Não é permitido destruir o heap padrão"};
    }
    auto it = secondary_heaps_.find(heap_handle);
    if (it == secondary_heaps_.end()) {
        return Error{ErrorCode::InvalidArgument, "Handle de heap inexistente"};
    }
    secondary_heaps_.erase(it);
    return Result<void>::Ok();
}

Result<GuestHeap*> GuestHeapManager::GetHeap(u32 heap_handle) {
    if (heap_handle == 0 || heap_handle == default_heap_.base_addr()) {
        return &default_heap_;
    }
    auto it = secondary_heaps_.find(heap_handle);
    if (it != secondary_heaps_.end()) {
        return it->second.get();
    }
    return Error{ErrorCode::InvalidArgument, "Handle de heap inválido"};
}

} // namespace xblob::kernel
