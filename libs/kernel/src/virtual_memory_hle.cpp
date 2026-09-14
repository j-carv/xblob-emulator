#include "xblob/kernel/virtual_memory_hle.hpp"

namespace xblob::kernel {

VirtualMemoryManager::VirtualMemoryManager(GuestAddr user_base, GuestAddr user_limit)
    : user_base_(user_base), user_limit_(user_limit), next_auto_addr_(user_base) {}

void VirtualMemoryManager::Reset() {
    regions_.clear();
    next_auto_addr_ = user_base_;
}

bool VirtualMemoryManager::OverlapsExisting(GuestAddr base, std::size_t size) const {
    if (size == 0) {
        return false;
    }
    GuestAddr end = base + static_cast<GuestAddr>(size);
    for (const auto& [r_base, region] : regions_) {
        GuestAddr r_end = r_base + static_cast<GuestAddr>(region.size);
        if (base < r_end && end > r_base) {
            return true;
        }
    }
    return false;
}

GuestAddr VirtualMemoryManager::FindFreeRange(std::size_t aligned_size) const {
    GuestAddr candidate = static_cast<GuestAddr>(AlignUp(next_auto_addr_, kAllocationGranularity));
    while (candidate + aligned_size <= user_limit_) {
        bool collision = false;
        for (const auto& [r_base, region] : regions_) {
            GuestAddr r_end = r_base + static_cast<GuestAddr>(region.size);
            if (candidate < r_end && (candidate + aligned_size) > r_base) {
                candidate = static_cast<GuestAddr>(AlignUp(r_end, kAllocationGranularity));
                collision = true;
                break;
            }
        }
        if (!collision) {
            return candidate;
        }
    }
    return 0; // No range found
}

Result<GuestAddr> VirtualMemoryManager::Allocate(GuestAddr requested_base, std::size_t size_bytes,
                                                 u32 alloc_type, u32 protect,
                                                 memory::AddressSpace& /*mem*/) {
    if (size_bytes == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de alocação virtual não pode ser zero"};
    }

    std::size_t aligned_size = AlignUp(size_bytes, kPageSize);

    // Check for overflow
    if (requested_base != 0) {
        if (requested_base + aligned_size < requested_base ||
            requested_base + aligned_size > user_limit_ || requested_base < user_base_) {
            return Error{ErrorCode::OutOfBounds,
                         "Faixa de alocação virtual excede limites ou overflow"};
        }
        // Base address must be page-aligned
        if ((requested_base % kPageSize) != 0) {
            return Error{ErrorCode::InvalidArgument,
                         "Endereço base solicitado deve ser alinhado por página"};
        }
    }

    GuestAddr target_base = requested_base;

    // Handle MEM_RESERVE / MEM_COMMIT
    if ((alloc_type & kMemReserve) != 0) {
        if (target_base == 0) {
            target_base = FindFreeRange(aligned_size);
            if (target_base == 0) {
                return Error{ErrorCode::LimitReached, "Espaço virtual esgotado"};
            }
        } else {
            // Check for collision with existing region
            if (OverlapsExisting(target_base, aligned_size)) {
                return Error{ErrorCode::InvalidArgument,
                             "Região virtual solicitada colide com mapeamento existente"};
            }
        }

        u32 state = (alloc_type & kMemCommit) != 0 ? kMemCommit : kMemReserve;
        VirtualRegion reg;
        reg.base_address = target_base;
        reg.size = aligned_size;
        reg.state = state;
        reg.protect = protect;
        reg.allocation_protect = protect;
        reg.type = kMemPrivate;

        regions_[target_base] = reg;
        if (target_base >= next_auto_addr_) {
            next_auto_addr_ = target_base + static_cast<GuestAddr>(aligned_size);
        }
        return target_base;
    }

    if ((alloc_type & kMemCommit) != 0) {
        // Committing memory
        if (target_base == 0) {
            // Commit new region directly
            target_base = FindFreeRange(aligned_size);
            if (target_base == 0) {
                return Error{ErrorCode::LimitReached, "Espaço virtual esgotado"};
            }
            VirtualRegion reg;
            reg.base_address = target_base;
            reg.size = aligned_size;
            reg.state = kMemCommit;
            reg.protect = protect;
            reg.allocation_protect = protect;
            reg.type = kMemPrivate;

            regions_[target_base] = reg;
            if (target_base >= next_auto_addr_) {
                next_auto_addr_ = target_base + static_cast<GuestAddr>(aligned_size);
            }
            return target_base;
        }

        // Committing into existing reserved region or new region
        auto it = regions_.find(target_base);
        if (it != regions_.end()) {
            if (aligned_size > it->second.size) {
                return Error{ErrorCode::LimitReached,
                             "Tamanho de commit excede tamanho da região reservada"};
            }
            it->second.state = kMemCommit;
            it->second.protect = protect;
            return target_base;
        }

        // Commit on non-reserved address must not overlap other regions
        if (OverlapsExisting(target_base, aligned_size)) {
            return Error{ErrorCode::InvalidArgument,
                         "Commit sobrepõe região reservada existente de forma inconsistente"};
        }

        VirtualRegion reg;
        reg.base_address = target_base;
        reg.size = aligned_size;
        reg.state = kMemCommit;
        reg.protect = protect;
        reg.allocation_protect = protect;
        reg.type = kMemPrivate;

        regions_[target_base] = reg;
        if (target_base >= next_auto_addr_) {
            next_auto_addr_ = target_base + static_cast<GuestAddr>(aligned_size);
        }
        return target_base;
    }

    return Error{ErrorCode::InvalidArgument, "Tipo de alocação virtual inválido"};
}

Result<void> VirtualMemoryManager::Free(GuestAddr base, std::size_t size_bytes, u32 free_type,
                                        memory::AddressSpace& /*mem*/) {
    auto it = regions_.find(base);
    if (it == regions_.end()) {
        return Error{ErrorCode::InvalidArgument,
                     "Endereço base não corresponde a uma alocação virtual ativa"};
    }

    if ((free_type & kMemRelease) != 0) {
        // According to NT rules: when releasing, size must be 0 or equal to region size
        if (size_bytes != 0 && size_bytes != it->second.size) {
            return Error{ErrorCode::InvalidArgument,
                         "Tamanho deve ser 0 ao liberar região completa (MEM_RELEASE)"};
        }
        regions_.erase(it);
        return Result<void>::Ok();
    }

    if ((free_type & kMemDecommit) != 0) {
        std::size_t aligned_size = AlignUp(size_bytes, kPageSize);
        if (aligned_size > it->second.size) {
            return Error{ErrorCode::OutOfBounds,
                         "Tamanho de descomprometimento excede tamanho da região"};
        }
        it->second.state = kMemReserve;
        it->second.protect = kPageNoAccess;
        return Result<void>::Ok();
    }

    return Error{ErrorCode::InvalidArgument, "Tipo de liberação virtual inválido"};
}

Result<MemoryBasicInformation32> VirtualMemoryManager::Query(GuestAddr addr) const {
    MemoryBasicInformation32 info;
    info.base_address = static_cast<u32>(addr & ~(kPageSize - 1));

    // Find region containing addr
    for (const auto& [r_base, region] : regions_) {
        GuestAddr r_end = r_base + static_cast<GuestAddr>(region.size);
        if (addr >= r_base && addr < r_end) {
            info.allocation_base = static_cast<u32>(region.base_address);
            info.allocation_protect = region.allocation_protect;
            info.region_size = static_cast<u32>(region.size - (addr - r_base));
            info.state = region.state;
            info.protect = region.protect;
            info.type = region.type;
            return info;
        }
    }

    // Free unallocated space
    info.allocation_base = 0;
    info.allocation_protect = 0;
    info.region_size = static_cast<u32>(kPageSize);
    info.state = kMemFree;
    info.protect = kPageNoAccess;
    info.type = 0;
    return info;
}

Result<u32> VirtualMemoryManager::Protect(GuestAddr base, std::size_t size_bytes, u32 new_protect) {
    auto it = regions_.find(base);
    if (it == regions_.end()) {
        return Error{ErrorCode::InvalidArgument, "Região virtual não encontrada para proteção"};
    }
    std::size_t aligned_size = AlignUp(size_bytes, kPageSize);
    if (aligned_size > it->second.size) {
        return Error{ErrorCode::OutOfBounds, "Tamanho excede região virtual existente"};
    }
    u32 old_protect = it->second.protect;
    it->second.protect = new_protect;
    return old_protect;
}

bool VirtualMemoryManager::IsRangeCommitted(GuestAddr base, std::size_t size) const noexcept {
    if (size == 0) {
        return true;
    }
    GuestAddr end = base + static_cast<GuestAddr>(size);
    for (const auto& [r_base, region] : regions_) {
        GuestAddr r_end = r_base + static_cast<GuestAddr>(region.size);
        if (base >= r_base && end <= r_end && region.state == kMemCommit) {
            return true;
        }
    }
    return false;
}

} // namespace xblob::kernel
