#pragma once

#include "xblob/common/types.hpp"
#include "xblob/formats/media_type.hpp"

#include <cstdint>
#include <string>

namespace xblob {

// Public documentation references (clean-room provenance):
// - XboxDevWiki: "Xbox DVD File System (XDVDFS)"
// - MultimediaWiki: "XDVDFS"
// - extract-xiso / XboxKit open-source specifications
// Sector size on Xbox DVD media is 2048 bytes (0x800).
// Volume descriptor is located at sector 32 (0x20) relative to partition start.
// Trimmed XISO partition begins at sector 0 (offset 0).
// Raw Redump XGD1 game partition begins at sector 0x30600 (offset 0x18300000 = 405,798,912 bytes).

constexpr u64 kXdvdfsSectorSize = 2048;
constexpr u64 kXdvdfsPartitionStartTrimmed = 0x0ULL;
constexpr u64 kXdvdfsPartitionStartRaw = 0x30600ULL * kXdvdfsSectorSize; // 405,798,912 bytes

constexpr u64 kXdvdfsDescriptorSector = 32;
constexpr u64 kXdvdfsDescriptorFooterOffset = 0x7EC; // 2028 bytes into descriptor sector

// FAT-compatible attribute flags as used in XDVDFS directory entries
enum class XdvdfsFileAttribute : u8 {
    ReadOnly = 0x01,
    Hidden = 0x02,
    System = 0x04,
    Directory = 0x10,
    Archive = 0x20,
    Normal = 0x80,
};

[[nodiscard]] constexpr bool IsDirectoryAttribute(u8 attr) noexcept {
    return (attr & static_cast<u8>(XdvdfsFileAttribute::Directory)) != 0;
}

[[nodiscard]] constexpr bool IsReadOnlyAttribute(u8 attr) noexcept {
    return (attr & static_cast<u8>(XdvdfsFileAttribute::ReadOnly)) != 0;
}

struct XdvdfsBudgetConfig {
    size_t max_depth{32};
    size_t max_entries_per_dir{4096};
    size_t max_total_entries{65536};
    size_t max_dir_size_bytes{16 * 1024 * 1024}; // 16 MiB
    size_t max_filename_length{255};
};

struct XdvdfsEntry {
    std::string name;
    u8 attributes{0};
    bool is_directory{false};
    u32 starting_sector{0};
    u32 file_size{0};
    u64 offset_in_image{0}; // Absolute byte offset within the underlying image source
};

struct XdvdfsVolumeInfo {
    MediaType variant{MediaType::Unknown};
    u64 partition_base_offset{0};
    u64 descriptor_offset{0};
    u32 root_dir_sector{0};
    u32 root_dir_size{0};
    u64 creation_timestamp{0};
    bool valid_footer_magic{false};
};

} // namespace xblob
