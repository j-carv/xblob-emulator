#include "xblob/memory/address_space.hpp"

#include "xblob/common/safe_math.hpp"

#include <algorithm>

namespace xblob::memory {

Result<void> AddressSpace::MapRam(GuestAddr base, GuestSize size, Ram& ram, std::size_t ram_offset,
                                  MemoryPermission perms) {
    if (size == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de mapeamento não pode ser zero", base};
    }

    u64 guest_end = 0;
    if (!CheckedAdd(static_cast<u64>(base), static_cast<u64>(size), guest_end) ||
        guest_end > 0x100000000ULL) {
        return Error{ErrorCode::OutOfBounds,
                     "Faixa de mapeamento excede o espaço de endereçamento de 32 bits", base};
    }

    u64 ram_end = 0;
    if (!CheckedAdd(static_cast<u64>(ram_offset), static_cast<u64>(size), ram_end) ||
        ram_end > ram.size()) {
        return Error{ErrorCode::OutOfBounds, "Faixa de mapeamento excede os limites da RAM física",
                     ram_offset};
    }

    for (const auto& r : regions_) {
        if (r.Overlaps(base, size)) {
            return Error{ErrorCode::RegionOverlap,
                         "Mapeamento sobrepõe região de memória existente", base};
        }
    }

    Region reg{};
    reg.base = base;
    reg.size = size;
    reg.permissions = perms;
    reg.type = RegionType::Ram;
    reg.ram = &ram;
    reg.ram_offset = ram_offset;

    regions_.push_back(std::move(reg));
    std::sort(regions_.begin(), regions_.end(),
              [](const Region& a, const Region& b) { return a.base < b.base; });

    return Result<void>::Ok();
}

Result<void> AddressSpace::MapMmio(GuestAddr base, GuestSize size, MmioHandler handler,
                                   MemoryPermission perms) {
    if (size == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de mapeamento não pode ser zero", base};
    }

    u64 guest_end = 0;
    if (!CheckedAdd(static_cast<u64>(base), static_cast<u64>(size), guest_end) ||
        guest_end > 0x100000000ULL) {
        return Error{ErrorCode::OutOfBounds,
                     "Faixa de mapeamento excede o espaço de endereçamento de 32 bits", base};
    }

    for (const auto& r : regions_) {
        if (r.Overlaps(base, size)) {
            return Error{ErrorCode::RegionOverlap,
                         "Mapeamento sobrepõe região de memória existente", base};
        }
    }

    Region reg{};
    reg.base = base;
    reg.size = size;
    reg.permissions = perms;
    reg.type = RegionType::Mmio;
    reg.mmio_handler = std::move(handler);

    regions_.push_back(std::move(reg));
    std::sort(regions_.begin(), regions_.end(),
              [](const Region& a, const Region& b) { return a.base < b.base; });

    return Result<void>::Ok();
}

Result<std::reference_wrapper<const Region>>
AddressSpace::ResolveRegion(GuestAddr addr, GuestSize len, MemoryPermission required_perm) const {
    if (len == 0) {
        return Error{ErrorCode::InvalidArgument, "Comprimento de acesso não pode ser zero", addr};
    }

    u64 end_addr = 0;
    if (!CheckedAdd(static_cast<u64>(addr), static_cast<u64>(len), end_addr) ||
        end_addr > 0x100000000ULL) {
        return Error{ErrorCode::OutOfBounds, "Acesso ultrapassa o espaço de 32 bits", addr};
    }

    for (const auto& r : regions_) {
        if (addr >= r.base && addr < r.end()) {
            if (!r.ContainsRange(addr, len)) {
                return Error{ErrorCode::OutOfBounds, "Acesso cruza o limite da região de memória",
                             addr};
            }
            if (!HasPermission(r.permissions, required_perm)) {
                return Error{ErrorCode::AccessViolation,
                             "Violação de permissão de acesso à memória", addr};
            }
            return std::cref(r);
        }
    }

    return Error{ErrorCode::UnmappedAddress, "Endereço não mapeado", addr};
}

Result<std::reference_wrapper<Region>>
AddressSpace::ResolveRegionMut(GuestAddr addr, GuestSize len, MemoryPermission required_perm) {
    if (len == 0) {
        return Error{ErrorCode::InvalidArgument, "Comprimento de acesso não pode ser zero", addr};
    }

    u64 end_addr = 0;
    if (!CheckedAdd(static_cast<u64>(addr), static_cast<u64>(len), end_addr) ||
        end_addr > 0x100000000ULL) {
        return Error{ErrorCode::OutOfBounds, "Acesso ultrapassa o espaço de 32 bits", addr};
    }

    for (auto& r : regions_) {
        if (addr >= r.base && addr < r.end()) {
            if (!r.ContainsRange(addr, len)) {
                return Error{ErrorCode::OutOfBounds, "Acesso cruza o limite da região de memória",
                             addr};
            }
            if (!HasPermission(r.permissions, required_perm)) {
                return Error{ErrorCode::AccessViolation,
                             "Violação de permissão de acesso à memória", addr};
            }
            return std::ref(r);
        }
    }

    return Error{ErrorCode::UnmappedAddress, "Endereço não mapeado", addr};
}

Result<u8> AddressSpace::Read8(GuestAddr addr) const {
    auto reg_res = ResolveRegion(addr, 1, MemoryPermission::Read);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        return reg.ram->Read8(offset);
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->read) {
        u32 rel_offset = addr - reg.base;
        auto mmio_res = reg.mmio_handler->read(rel_offset, AccessWidth::Byte);
        if (!mmio_res) {
            return mmio_res.error();
        }
        return static_cast<u8>(mmio_res.value() & 0xff);
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de leitura MMIO não configurada", addr};
}

Result<u16> AddressSpace::Read16(GuestAddr addr) const {
    auto reg_res = ResolveRegion(addr, 2, MemoryPermission::Read);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        const u8* ptr = reg.ram->data() + offset;
        u16 val = static_cast<u16>(static_cast<u16>(ptr[0]) | (static_cast<u16>(ptr[1]) << 8));
        return val;
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->read) {
        u32 rel_offset = addr - reg.base;
        auto mmio_res = reg.mmio_handler->read(rel_offset, AccessWidth::Word);
        if (!mmio_res) {
            return mmio_res.error();
        }
        return static_cast<u16>(mmio_res.value() & 0xffff);
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de leitura MMIO não configurada", addr};
}

Result<u32> AddressSpace::Read32(GuestAddr addr) const {
    auto reg_res = ResolveRegion(addr, 4, MemoryPermission::Read);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        const u8* ptr = reg.ram->data() + offset;
        u32 val = static_cast<u32>(ptr[0]) | (static_cast<u32>(ptr[1]) << 8) |
                  (static_cast<u32>(ptr[2]) << 16) | (static_cast<u32>(ptr[3]) << 24);
        return val;
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->read) {
        u32 rel_offset = addr - reg.base;
        auto mmio_res = reg.mmio_handler->read(rel_offset, AccessWidth::Dword);
        if (!mmio_res) {
            return mmio_res.error();
        }
        return mmio_res.value();
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de leitura MMIO não configurada", addr};
}

Result<void> AddressSpace::Write8(GuestAddr addr, u8 value) {
    auto reg_res = ResolveRegionMut(addr, 1, MemoryPermission::Write);
    if (!reg_res) {
        return reg_res.error();
    }
    Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        return reg.ram->Write8(offset, value);
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->write) {
        u32 rel_offset = addr - reg.base;
        return reg.mmio_handler->write(rel_offset, AccessWidth::Byte, value);
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de escrita MMIO não configurada", addr};
}

Result<void> AddressSpace::Write16(GuestAddr addr, u16 value) {
    auto reg_res = ResolveRegionMut(addr, 2, MemoryPermission::Write);
    if (!reg_res) {
        return reg_res.error();
    }
    Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        u8* ptr = reg.ram->data() + offset;
        ptr[0] = static_cast<u8>(value & 0xff);
        ptr[1] = static_cast<u8>((value >> 8) & 0xff);
        return Result<void>::Ok();
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->write) {
        u32 rel_offset = addr - reg.base;
        return reg.mmio_handler->write(rel_offset, AccessWidth::Word, value);
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de escrita MMIO não configurada", addr};
}

Result<void> AddressSpace::Write32(GuestAddr addr, u32 value) {
    auto reg_res = ResolveRegionMut(addr, 4, MemoryPermission::Write);
    if (!reg_res) {
        return reg_res.error();
    }
    Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        u8* ptr = reg.ram->data() + offset;
        ptr[0] = static_cast<u8>(value & 0xff);
        ptr[1] = static_cast<u8>((value >> 8) & 0xff);
        ptr[2] = static_cast<u8>((value >> 16) & 0xff);
        ptr[3] = static_cast<u8>((value >> 24) & 0xff);
        return Result<void>::Ok();
    }

    if (reg.type == RegionType::Mmio && reg.mmio_handler && reg.mmio_handler->write) {
        u32 rel_offset = addr - reg.base;
        return reg.mmio_handler->write(rel_offset, AccessWidth::Dword, value);
    }

    return Error{ErrorCode::UnsupportedFeature, "Operação de escrita MMIO não configurada", addr};
}

Result<u8> AddressSpace::Fetch8(GuestAddr addr) const {
    auto reg_res = ResolveRegion(addr, 1, MemoryPermission::Execute);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        return reg.ram->Read8(offset);
    }

    return Error{ErrorCode::UnsupportedFeature, "Fetch de instrução não suportado em MMIO", addr};
}

Result<void> AddressSpace::FetchBytes(GuestAddr addr, MutableByteSpan dest) const {
    if (dest.empty()) {
        return Result<void>::Ok();
    }

    auto reg_res =
        ResolveRegion(addr, static_cast<GuestSize>(dest.size()), MemoryPermission::Execute);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        const u8* src = reg.ram->data() + offset;
        std::copy(src, src + dest.size(), dest.begin());
        return Result<void>::Ok();
    }

    return Error{ErrorCode::UnsupportedFeature, "Fetch de instrução não suportado em MMIO", addr};
}

Result<void> AddressSpace::ReadBytes(GuestAddr addr, MutableByteSpan dest) const {
    if (dest.empty()) {
        return Result<void>::Ok();
    }

    auto reg_res = ResolveRegion(addr, static_cast<GuestSize>(dest.size()), MemoryPermission::Read);
    if (!reg_res) {
        return reg_res.error();
    }
    const Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        const u8* src = reg.ram->data() + offset;
        std::copy(src, src + dest.size(), dest.begin());
        return Result<void>::Ok();
    }

    return Error{ErrorCode::UnsupportedFeature, "ReadBytes não suportado em MMIO", addr};
}

Result<void> AddressSpace::WriteBytes(GuestAddr addr, ByteSpan src) {
    if (src.empty()) {
        return Result<void>::Ok();
    }

    auto reg_res =
        ResolveRegionMut(addr, static_cast<GuestSize>(src.size()), MemoryPermission::Write);
    if (!reg_res) {
        return reg_res.error();
    }
    Region& reg = reg_res.value().get();

    if (reg.type == RegionType::Ram) {
        std::size_t offset = reg.ram_offset + (addr - reg.base);
        u8* dest = reg.ram->data() + offset;
        std::copy(src.begin(), src.end(), dest);
        return Result<void>::Ok();
    }

    return Error{ErrorCode::UnsupportedFeature, "WriteBytes não suportado em MMIO", addr};
}

} // namespace xblob::memory
