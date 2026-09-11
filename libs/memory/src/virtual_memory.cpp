#include "xblob/memory/virtual_memory.hpp"

namespace xblob::memory {

VirtualMemory::VirtualMemory(AddressSpace& physical_space) : physical_space_(physical_space) {}

void VirtualMemory::SetCr0(u32 cr0) noexcept {
    const bool paging_changed = ((cr0_ ^ cr0) & kCr0Paging) != 0;
    cr0_ = cr0;
    if (paging_changed) {
        InvalidateAll();
    }
}

void VirtualMemory::SetCr3(u32 cr3) noexcept {
    cr3_ = cr3;
    InvalidateAll(); // IA-32 flushes TLB on CR3 reload
}

void VirtualMemory::InvalidateTranslation(GuestAddr vaddr) noexcept {
    const u32 page_num = vaddr >> 12;
    tlb_.erase(page_num);
}

void VirtualMemory::InvalidateAll() noexcept {
    tlb_.clear();
}

void VirtualMemory::RecordFault(GuestAddr vaddr, u32 error_code,
                                VirtualAccessType access_type) noexcept {
    last_page_fault_ = PageFault{
        .fault_address = vaddr,
        .error_code = error_code,
        .access_type = access_type,
        .cpl = cpl_,
    };
}

Result<TranslationResult> VirtualMemory::Translate(GuestAddr vaddr, VirtualAccessType access_type) {
    if (!is_paging_enabled()) {
        return TranslationResult{
            .physical_address = vaddr,
            .effective_permissions = MemoryPermission::All,
        };
    }

    const u32 page_num = vaddr >> 12;
    const u32 offset = vaddr & 0xFFF;

    auto tlb_it = tlb_.find(page_num);
    if (tlb_it != tlb_.end()) {
        const auto& entry = tlb_it->second;
        if (cpl_ == 3) {
            if (!entry.user_accessible) {
                u32 code = kPageFaultPresent | kPageFaultUser;
                if (access_type == VirtualAccessType::Write)
                    code |= kPageFaultWrite;
                if (access_type == VirtualAccessType::Execute)
                    code |= kPageFaultInstruction;
                RecordFault(vaddr, code, access_type);
                return Error{ErrorCode::AccessViolation, "Violação de privilégio (supervisor)",
                             vaddr};
            }
            if (access_type == VirtualAccessType::Write && !entry.writable) {
                u32 code = kPageFaultPresent | kPageFaultWrite | kPageFaultUser;
                RecordFault(vaddr, code, access_type);
                return Error{ErrorCode::AccessViolation,
                             "Violação de proteção de escrita em usuário", vaddr};
            }
        } else {
            if (access_type == VirtualAccessType::Write && is_write_protect() && !entry.writable) {
                u32 code = kPageFaultPresent | kPageFaultWrite;
                RecordFault(vaddr, code, access_type);
                return Error{ErrorCode::AccessViolation,
                             "Violação de proteção de escrita em supervisor", vaddr};
            }
        }
        return TranslationResult{
            .physical_address = (entry.physical_page << 12) | offset,
            .effective_permissions = entry.effective_permissions,
        };
    }

    // Page walk (Directory and Table)
    const u32 pde_idx = (vaddr >> 22) & 0x3FF;
    const u32 pte_idx = (vaddr >> 12) & 0x3FF;

    const GuestAddr pde_addr = (cr3_ & kPageBaseMask) + (pde_idx * 4);
    auto pde_res = physical_space_.Read32(pde_addr);
    if (!pde_res) {
        return pde_res.error();
    }

    const u32 pde = *pde_res;
    if (!(pde & kPagePresent)) {
        u32 code = 0; // Not present
        if (access_type == VirtualAccessType::Write)
            code |= kPageFaultWrite;
        if (cpl_ == 3)
            code |= kPageFaultUser;
        if (access_type == VirtualAccessType::Execute)
            code |= kPageFaultInstruction;
        RecordFault(vaddr, code, access_type);
        return Error{ErrorCode::AccessViolation, "Entrada de diretório de páginas não presente",
                     vaddr};
    }

    const GuestAddr pte_addr = (pde & kPageBaseMask) + (pte_idx * 4);
    auto pte_res = physical_space_.Read32(pte_addr);
    if (!pte_res) {
        return pte_res.error();
    }

    const u32 pte = *pte_res;
    if (!(pte & kPagePresent)) {
        u32 code = 0; // Not present
        if (access_type == VirtualAccessType::Write)
            code |= kPageFaultWrite;
        if (cpl_ == 3)
            code |= kPageFaultUser;
        if (access_type == VirtualAccessType::Execute)
            code |= kPageFaultInstruction;
        RecordFault(vaddr, code, access_type);
        return Error{ErrorCode::AccessViolation, "Entrada de tabela de páginas não presente",
                     vaddr};
    }

    const bool user_accessible = (pde & kPageUser) && (pte & kPageUser);
    const bool writable = (pde & kPageWritable) && (pte & kPageWritable);

    if (cpl_ == 3) {
        if (!user_accessible) {
            u32 code = kPageFaultPresent | kPageFaultUser;
            if (access_type == VirtualAccessType::Write)
                code |= kPageFaultWrite;
            if (access_type == VirtualAccessType::Execute)
                code |= kPageFaultInstruction;
            RecordFault(vaddr, code, access_type);
            return Error{ErrorCode::AccessViolation, "Violação de privilégio (supervisor)", vaddr};
        }
        if (access_type == VirtualAccessType::Write && !writable) {
            u32 code = kPageFaultPresent | kPageFaultWrite | kPageFaultUser;
            RecordFault(vaddr, code, access_type);
            return Error{ErrorCode::AccessViolation, "Violação de proteção de escrita em usuário",
                         vaddr};
        }
    } else {
        if (access_type == VirtualAccessType::Write && is_write_protect() && !writable) {
            u32 code = kPageFaultPresent | kPageFaultWrite;
            RecordFault(vaddr, code, access_type);
            return Error{ErrorCode::AccessViolation,
                         "Violação de proteção de escrita em supervisor", vaddr};
        }
    }

    MemoryPermission perms = MemoryPermission::Read | MemoryPermission::Execute;
    if (writable) {
        perms = perms | MemoryPermission::Write;
    }

    const u32 phys_page = (pte & kPageBaseMask) >> 12;
    tlb_[page_num] = TlbEntry{
        .physical_page = phys_page,
        .effective_permissions = perms,
        .user_accessible = user_accessible,
        .writable = writable,
    };

    return TranslationResult{
        .physical_address = (phys_page << 12) | offset,
        .effective_permissions = perms,
    };
}

Result<u8> VirtualMemory::Read8(GuestAddr vaddr) {
    auto trans = Translate(vaddr, VirtualAccessType::Read);
    if (!trans)
        return trans.error();
    return physical_space_.Read8(trans->physical_address);
}

Result<u16> VirtualMemory::Read16(GuestAddr vaddr) {
    if ((vaddr & 0xFFF) <= 0xFFE) {
        auto trans = Translate(vaddr, VirtualAccessType::Read);
        if (!trans)
            return trans.error();
        return physical_space_.Read16(trans->physical_address);
    }
    auto b0 = Read8(vaddr);
    if (!b0)
        return b0.error();
    auto b1 = Read8(vaddr + 1);
    if (!b1)
        return b1.error();
    return static_cast<u16>(static_cast<u32>(*b0) | (static_cast<u32>(*b1) << 8));
}

Result<u32> VirtualMemory::Read32(GuestAddr vaddr) {
    if ((vaddr & 0xFFF) <= 0xFFC) {
        auto trans = Translate(vaddr, VirtualAccessType::Read);
        if (!trans)
            return trans.error();
        return physical_space_.Read32(trans->physical_address);
    }
    auto b0 = Read8(vaddr);
    if (!b0)
        return b0.error();
    auto b1 = Read8(vaddr + 1);
    if (!b1)
        return b1.error();
    auto b2 = Read8(vaddr + 2);
    if (!b2)
        return b2.error();
    auto b3 = Read8(vaddr + 3);
    if (!b3)
        return b3.error();
    return static_cast<u32>(*b0) | (static_cast<u32>(*b1) << 8) | (static_cast<u32>(*b2) << 16) |
           (static_cast<u32>(*b3) << 24);
}

Result<void> VirtualMemory::Write8(GuestAddr vaddr, u8 value) {
    auto trans = Translate(vaddr, VirtualAccessType::Write);
    if (!trans)
        return trans.error();
    return physical_space_.Write8(trans->physical_address, value);
}

Result<void> VirtualMemory::Write16(GuestAddr vaddr, u16 value) {
    if ((vaddr & 0xFFF) <= 0xFFE) {
        auto trans = Translate(vaddr, VirtualAccessType::Write);
        if (!trans)
            return trans.error();
        return physical_space_.Write16(trans->physical_address, value);
    }
    // Transactional write across page boundary: validate both translations first
    auto trans0 = Translate(vaddr, VirtualAccessType::Write);
    if (!trans0)
        return trans0.error();
    auto trans1 = Translate(vaddr + 1, VirtualAccessType::Write);
    if (!trans1)
        return trans1.error();

    auto res0 = physical_space_.Write8(trans0->physical_address, static_cast<u8>(value & 0xFF));
    if (!res0)
        return res0.error();
    return physical_space_.Write8(trans1->physical_address, static_cast<u8>((value >> 8) & 0xFF));
}

Result<void> VirtualMemory::Write32(GuestAddr vaddr, u32 value) {
    if ((vaddr & 0xFFF) <= 0xFFC) {
        auto trans = Translate(vaddr, VirtualAccessType::Write);
        if (!trans)
            return trans.error();
        return physical_space_.Write32(trans->physical_address, value);
    }
    // Transactional write: check all 4 byte translations first
    GuestAddr phys[4];
    for (u32 i = 0; i < 4; ++i) {
        auto trans = Translate(vaddr + i, VirtualAccessType::Write);
        if (!trans)
            return trans.error();
        phys[i] = trans->physical_address;
    }
    for (u32 i = 0; i < 4; ++i) {
        auto res = physical_space_.Write8(phys[i], static_cast<u8>((value >> (i * 8)) & 0xFF));
        if (!res)
            return res.error();
    }
    return {};
}

Result<u8> VirtualMemory::Fetch8(GuestAddr vaddr) {
    auto trans = Translate(vaddr, VirtualAccessType::Execute);
    if (!trans)
        return trans.error();
    return physical_space_.Fetch8(trans->physical_address);
}

Result<void> VirtualMemory::FetchBytes(GuestAddr vaddr, MutableByteSpan dest) {
    for (std::size_t i = 0; i < dest.size(); ++i) {
        auto b = Fetch8(vaddr + static_cast<u32>(i));
        if (!b)
            return b.error();
        dest[i] = *b;
    }
    return {};
}

Result<void> VirtualMemory::ReadBytes(GuestAddr vaddr, MutableByteSpan dest) {
    for (std::size_t i = 0; i < dest.size(); ++i) {
        auto b = Read8(vaddr + static_cast<u32>(i));
        if (!b)
            return b.error();
        dest[i] = *b;
    }
    return {};
}

Result<void> VirtualMemory::WriteBytes(GuestAddr vaddr, ByteSpan src) {
    // Transactional: validate write permission on all spanned pages first
    for (std::size_t i = 0; i < src.size(); ++i) {
        auto trans = Translate(vaddr + static_cast<u32>(i), VirtualAccessType::Write);
        if (!trans)
            return trans.error();
    }
    for (std::size_t i = 0; i < src.size(); ++i) {
        auto res = Write8(vaddr + static_cast<u32>(i), src[i]);
        if (!res)
            return res.error();
    }
    return {};
}

} // namespace xblob::memory
