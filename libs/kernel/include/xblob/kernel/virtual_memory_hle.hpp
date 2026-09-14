#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/kernel/types.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/permission.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace xblob::kernel {

// Standard NT memory protection constants (public clean-room specification)
constexpr u32 kPageNoAccess = 0x01;
constexpr u32 kPageReadOnly = 0x02;
constexpr u32 kPageReadWrite = 0x04;
constexpr u32 kPageWriteCopy = 0x08;
constexpr u32 kPageExecute = 0x10;
constexpr u32 kPageExecuteRead = 0x20;
constexpr u32 kPageExecuteReadWrite = 0x40;
constexpr u32 kPageExecuteWriteCopy = 0x80;
constexpr u32 kPageGuard = 0x100;
constexpr u32 kPageNoCache = 0x200;
constexpr u32 kPageWriteCombine = 0x400;

// Standard NT allocation type constants
constexpr u32 kMemCommit = 0x1000;
constexpr u32 kMemReserve = 0x2000;
constexpr u32 kMemDecommit = 0x4000;
constexpr u32 kMemRelease = 0x8000;
constexpr u32 kMemFree = 0x10000;
constexpr u32 kMemPrivate = 0x20000;
constexpr u32 kMemReset = 0x80000;
constexpr u32 kMemTopDown = 0x100000;

struct MemoryBasicInformation32 {
    u32 base_address{0};
    u32 allocation_base{0};
    u32 allocation_protect{0};
    u32 region_size{0};
    u32 state{kMemFree};
    u32 protect{kPageNoAccess};
    u32 type{0};
};

struct VirtualRegion {
    GuestAddr base_address{0};
    std::size_t size{0};
    u32 state{kMemFree};
    u32 protect{kPageNoAccess};
    u32 allocation_protect{kPageNoAccess};
    u32 type{kMemPrivate};
};

class VirtualMemoryManager {
public:
    static constexpr std::size_t kPageSize = 4096;
    static constexpr std::size_t kAllocationGranularity = 64 * 1024;
    static constexpr GuestAddr kDefaultUserBase = 0x00010000;
    static constexpr GuestAddr kDefaultUserLimit = 0xF0000000;

    explicit VirtualMemoryManager(GuestAddr user_base = kDefaultUserBase,
                                  GuestAddr user_limit = kDefaultUserLimit);

    void Reset();

    Result<GuestAddr> Allocate(GuestAddr requested_base, std::size_t size_bytes, u32 alloc_type,
                               u32 protect, memory::AddressSpace& mem);

    Result<void> Free(GuestAddr base, std::size_t size_bytes, u32 free_type,
                      memory::AddressSpace& mem);

    Result<MemoryBasicInformation32> Query(GuestAddr addr) const;

    Result<u32> Protect(GuestAddr base, std::size_t size_bytes, u32 new_protect);

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }
    [[nodiscard]] bool IsRangeCommitted(GuestAddr base, std::size_t size) const noexcept;

private:
    GuestAddr user_base_;
    GuestAddr user_limit_;
    GuestAddr next_auto_addr_;
    std::map<GuestAddr, VirtualRegion> regions_;

    [[nodiscard]] GuestAddr FindFreeRange(std::size_t aligned_size) const;
    [[nodiscard]] bool OverlapsExisting(GuestAddr base, std::size_t size) const;
    static constexpr std::size_t AlignUp(std::size_t val, std::size_t align) noexcept {
        return (val + align - 1) & ~(align - 1);
    }
};

} // namespace xblob::kernel
