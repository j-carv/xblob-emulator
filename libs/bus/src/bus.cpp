#include "xblob/bus/bus.hpp"

#include <algorithm>

namespace xblob::bus {

Result<void> Bus::MapDevice(GuestAddr base, GuestSize size, std::shared_ptr<BusDevice> device) {
    if (!device) {
        return Error{ErrorCode::InvalidArgument, "Dispositivo nulo no mapeamento de barramento",
                     base};
    }
    if (size == 0) {
        return Error{ErrorCode::InvalidArgument, "Tamanho de range não pode ser zero", base};
    }

    const u64 start = base;
    const u64 range_end = start + static_cast<u64>(size);
    if (range_end > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow,
                     "Faixa de barramento extrapola espaço de endereçamento de 32 bits", base};
    }

    for (const auto& mapping : mappings_) {
        if (mapping.Overlaps(base, size)) {
            return Error{ErrorCode::RegionOverlap,
                         "Faixa do dispositivo sobrepõe faixa previamente registrada", base};
        }
    }

    mappings_.push_back(BusMapping{base, size, std::move(device)});
    return {};
}

Result<void> Bus::UnmapDevice(GuestAddr base) {
    auto it = std::find_if(mappings_.begin(), mappings_.end(),
                           [base](const BusMapping& m) { return m.base == base; });
    if (it == mappings_.end()) {
        return Error{ErrorCode::UnmappedAddress, "Nenhum dispositivo mapeado na base informada",
                     base};
    }
    mappings_.erase(it);
    return {};
}

Result<u32> Bus::Read(GuestAddr addr, BusAccessWidth width) {
    const u32 w = static_cast<u32>(width);
    if (w != 1 && w != 2 && w != 4) {
        return Error{ErrorCode::UnsupportedAccessSize, "Largura de acesso MMIO não suportada",
                     addr};
    }

    const u64 access_end = static_cast<u64>(addr) + static_cast<u64>(w);
    if (access_end > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow, "Acesso MMIO cruza limite de 32 bits", addr};
    }

    for (const auto& mapping : mappings_) {
        if (mapping.Contains(addr, width)) {
            const u32 offset = addr - mapping.base;
            return mapping.device->Read(offset, width);
        }
    }

    return Error{ErrorCode::UnmappedAddress, "Endereço sem dispositivo no barramento", addr};
}

Result<void> Bus::Write(GuestAddr addr, BusAccessWidth width, u32 value) {
    const u32 w = static_cast<u32>(width);
    if (w != 1 && w != 2 && w != 4) {
        return Error{ErrorCode::UnsupportedAccessSize, "Largura de acesso MMIO não suportada",
                     addr};
    }

    const u64 access_end = static_cast<u64>(addr) + static_cast<u64>(w);
    if (access_end > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow, "Acesso MMIO cruza limite de 32 bits", addr};
    }

    for (const auto& mapping : mappings_) {
        if (mapping.Contains(addr, width)) {
            const u32 offset = addr - mapping.base;
            return mapping.device->Write(offset, width, value);
        }
    }

    return Error{ErrorCode::UnmappedAddress, "Endereço sem dispositivo no barramento", addr};
}

} // namespace xblob::bus
