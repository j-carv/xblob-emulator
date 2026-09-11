#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/memory/mmio.hpp"
#include "xblob/memory/permission.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/memory/region.hpp"

#include <vector>

namespace xblob::memory {

class AddressSpace {
public:
    AddressSpace() = default;

    [[nodiscard]] Result<void> MapRam(GuestAddr base, GuestSize size, Ram& ram,
                                      std::size_t ram_offset,
                                      MemoryPermission perms = MemoryPermission::All);

    [[nodiscard]] Result<void> MapMmio(GuestAddr base, GuestSize size, MmioHandler handler,
                                       MemoryPermission perms = MemoryPermission::ReadWrite);

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }
    [[nodiscard]] const std::vector<Region>& regions() const noexcept { return regions_; }

    [[nodiscard]] Result<u8> Read8(GuestAddr addr) const;
    [[nodiscard]] Result<u16> Read16(GuestAddr addr) const;
    [[nodiscard]] Result<u32> Read32(GuestAddr addr) const;

    [[nodiscard]] Result<void> Write8(GuestAddr addr, u8 value);
    [[nodiscard]] Result<void> Write16(GuestAddr addr, u16 value);
    [[nodiscard]] Result<void> Write32(GuestAddr addr, u32 value);

    [[nodiscard]] Result<u8> Fetch8(GuestAddr addr) const;
    [[nodiscard]] Result<void> FetchBytes(GuestAddr addr, MutableByteSpan dest) const;

    [[nodiscard]] Result<void> ReadBytes(GuestAddr addr, MutableByteSpan dest) const;
    [[nodiscard]] Result<void> WriteBytes(GuestAddr addr, ByteSpan src);
    [[nodiscard]] Result<void> ValidateRange(GuestAddr addr, GuestSize len,
                                             MemoryPermission required_perm) const;

private:
    [[nodiscard]] Result<std::reference_wrapper<const Region>>
    ResolveRegion(GuestAddr addr, GuestSize len, MemoryPermission required_perm) const;

    [[nodiscard]] Result<std::reference_wrapper<Region>>
    ResolveRegionMut(GuestAddr addr, GuestSize len, MemoryPermission required_perm);

    std::vector<Region> regions_;
};

} // namespace xblob::memory
