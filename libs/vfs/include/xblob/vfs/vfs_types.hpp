#pragma once

#include "xblob/common/types.hpp"

#include <cstdint>
#include <string>

namespace xblob {

struct VfsHandle {
    u16 index{0};
    u16 generation{0};

    [[nodiscard]] constexpr bool IsValid() const noexcept { return generation != 0; }

    [[nodiscard]] constexpr u32 ToU32() const noexcept {
        return (static_cast<u32>(generation) << 16) | static_cast<u32>(index);
    }

    [[nodiscard]] static constexpr VfsHandle FromU32(u32 val) noexcept {
        return VfsHandle{
            .index = static_cast<u16>(val & 0xFFFF),
            .generation = static_cast<u16>((val >> 16) & 0xFFFF),
        };
    }

    constexpr bool operator==(const VfsHandle& other) const noexcept = default;
};

enum class VfsNodeType : u8 {
    File,
    Directory,
};

enum class VfsAccessMode : u8 {
    Read = 0x01,
    Write = 0x02,
};

enum class VfsSeekOrigin : u8 {
    Begin,
    Current,
    End,
};

struct VfsFileInfo {
    std::string name;
    u64 size{0};
    bool is_directory{false};
    u32 attributes{0};
    u64 creation_timestamp{0};
};

struct VfsDirEntry {
    std::string name;
    bool is_directory{false};
    u64 size{0};
    u32 attributes{0};
};

} // namespace xblob
