#pragma once

#include "xblob/common/types.hpp"

namespace xblob::memory {

enum class MemoryPermission : u8 {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    Execute = 1 << 2,
    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    All = Read | Write | Execute
};

[[nodiscard]] constexpr MemoryPermission operator|(MemoryPermission a,
                                                   MemoryPermission b) noexcept {
    return static_cast<MemoryPermission>(static_cast<u8>(a) | static_cast<u8>(b));
}

[[nodiscard]] constexpr MemoryPermission operator&(MemoryPermission a,
                                                   MemoryPermission b) noexcept {
    return static_cast<MemoryPermission>(static_cast<u8>(a) & static_cast<u8>(b));
}

[[nodiscard]] constexpr MemoryPermission operator~(MemoryPermission a) noexcept {
    return static_cast<MemoryPermission>(~static_cast<u8>(a));
}

[[nodiscard]] constexpr bool HasPermission(MemoryPermission perms,
                                           MemoryPermission required) noexcept {
    return (perms & required) == required;
}

} // namespace xblob::memory
