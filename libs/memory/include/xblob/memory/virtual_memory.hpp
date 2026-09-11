#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/permission.hpp"

#include <optional>
#include <unordered_map>

namespace xblob::memory {

// CR0 bit definitions
inline constexpr u32 kCr0Paging = 1u << 31;       // PG
inline constexpr u32 kCr0WriteProtect = 1u << 16; // WP

// Page table entry bit definitions (IA-32 4 KiB)
inline constexpr u32 kPagePresent = 1u << 0;
inline constexpr u32 kPageWritable = 1u << 1;
inline constexpr u32 kPageUser = 1u << 2;
inline constexpr u32 kPageWriteThrough = 1u << 3;
inline constexpr u32 kPageCacheDisable = 1u << 4;
inline constexpr u32 kPageAccessed = 1u << 5;
inline constexpr u32 kPageDirty = 1u << 6;
inline constexpr u32 kPageSize4Mb = 1u << 7;
inline constexpr u32 kPageBaseMask = 0xFFFFF000u;

// Access type for virtual memory translation
enum class VirtualAccessType : u8 { Read, Write, Execute };

// IA-32 architectural Page Fault Error Code bits
inline constexpr u32 kPageFaultPresent = 1u << 0;     // 0: non-present, 1: protection violation
inline constexpr u32 kPageFaultWrite = 1u << 1;       // 0: read/fetch, 1: write
inline constexpr u32 kPageFaultUser = 1u << 2;        // 0: supervisor (CPL 0), 1: user (CPL 3)
inline constexpr u32 kPageFaultReserved = 1u << 3;    // Reserved bit violation
inline constexpr u32 kPageFaultInstruction = 1u << 4; // 1: instruction fetch

struct PageFault {
    GuestAddr fault_address{0};
    u32 error_code{0};
    VirtualAccessType access_type{VirtualAccessType::Read};
    u8 cpl{0};

    [[nodiscard]] constexpr bool is_protection_violation() const noexcept {
        return (error_code & kPageFaultPresent) != 0;
    }
    [[nodiscard]] constexpr bool is_not_present() const noexcept {
        return (error_code & kPageFaultPresent) == 0;
    }
    [[nodiscard]] constexpr bool is_write() const noexcept {
        return (error_code & kPageFaultWrite) != 0;
    }
    [[nodiscard]] constexpr bool is_user() const noexcept {
        return (error_code & kPageFaultUser) != 0;
    }
    [[nodiscard]] constexpr bool is_instruction_fetch() const noexcept {
        return (error_code & kPageFaultInstruction) != 0;
    }
};

struct TranslationResult {
    GuestAddr physical_address{0};
    MemoryPermission effective_permissions{MemoryPermission::None};
};

class VirtualMemory {
public:
    explicit VirtualMemory(AddressSpace& physical_space);

    void SetCr0(u32 cr0) noexcept;
    [[nodiscard]] u32 cr0() const noexcept { return cr0_; }

    void SetCr3(u32 cr3) noexcept;
    [[nodiscard]] u32 cr3() const noexcept { return cr3_; }

    void SetCpl(u8 cpl) noexcept { cpl_ = cpl; }
    [[nodiscard]] u8 cpl() const noexcept { return cpl_; }

    [[nodiscard]] bool is_paging_enabled() const noexcept { return (cr0_ & kCr0Paging) != 0; }
    [[nodiscard]] bool is_write_protect() const noexcept { return (cr0_ & kCr0WriteProtect) != 0; }

    void InvalidateTranslation(GuestAddr vaddr) noexcept;
    void InvalidateAll() noexcept;

    [[nodiscard]] Result<TranslationResult> Translate(GuestAddr vaddr,
                                                      VirtualAccessType access_type);

    [[nodiscard]] Result<u8> Read8(GuestAddr vaddr);
    [[nodiscard]] Result<u16> Read16(GuestAddr vaddr);
    [[nodiscard]] Result<u32> Read32(GuestAddr vaddr);

    [[nodiscard]] Result<void> Write8(GuestAddr vaddr, u8 value);
    [[nodiscard]] Result<void> Write16(GuestAddr vaddr, u16 value);
    [[nodiscard]] Result<void> Write32(GuestAddr vaddr, u32 value);

    [[nodiscard]] Result<u8> Fetch8(GuestAddr vaddr);
    [[nodiscard]] Result<void> FetchBytes(GuestAddr vaddr, MutableByteSpan dest);

    [[nodiscard]] Result<void> ReadBytes(GuestAddr vaddr, MutableByteSpan dest);
    [[nodiscard]] Result<void> WriteBytes(GuestAddr vaddr, ByteSpan src);

    [[nodiscard]] const std::optional<PageFault>& last_page_fault() const noexcept {
        return last_page_fault_;
    }

    [[nodiscard]] AddressSpace& physical_space() noexcept { return physical_space_; }
    [[nodiscard]] const AddressSpace& physical_space() const noexcept { return physical_space_; }

private:
    struct TlbEntry {
        u32 physical_page{0};
        MemoryPermission effective_permissions{MemoryPermission::None};
        bool user_accessible{false};
        bool writable{false};
    };

    void RecordFault(GuestAddr vaddr, u32 error_code, VirtualAccessType access_type) noexcept;

    AddressSpace& physical_space_;
    u32 cr0_{0};
    u32 cr3_{0};
    u8 cpl_{0};
    std::optional<PageFault> last_page_fault_{std::nullopt};
    std::unordered_map<u32, TlbEntry> tlb_;
};

} // namespace xblob::memory
