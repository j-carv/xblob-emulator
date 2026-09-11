#include "xblob/formats/xdvdfs.hpp"

#include "xblob/common/safe_math.hpp"
#include "xblob/formats/xiso.hpp"
#include "xblob/io/binary_reader.hpp"
#include "xdvdfs_entry_reader.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace xblob {

namespace {

bool CheckMagicAt(const ByteSource& source, u64 offset, std::string_view expected) {
    if (!RangeInBoundsU64(offset, expected.size(), source.size())) {
        return false;
    }
    std::vector<u8> buf(expected.size());
    auto res = source.ReadAt(offset, std::span<u8>(buf));
    if (!res) {
        return false;
    }
    return std::memcmp(buf.data(), expected.data(), expected.size()) == 0;
}

} // namespace

XdvdfsVolume::XdvdfsVolume(std::shared_ptr<const ByteSource> source, XdvdfsVolumeInfo info,
                           XdvdfsBudgetConfig budget) noexcept
    : source_(std::move(source)), info_(info), budget_(budget) {}

Result<std::shared_ptr<XdvdfsVolume>> XdvdfsVolume::Open(std::shared_ptr<const ByteSource> source,
                                                         XdvdfsBudgetConfig budget) {
    if (!source) {
        return Error{ErrorCode::InvalidArgument, "ByteSource cannot be null"};
    }

    MediaType variant = MediaType::Unknown;
    u64 partition_base = 0;
    u64 desc_offset = 0;

    const u64 trimmed_desc =
        kXdvdfsPartitionStartTrimmed + (kXdvdfsDescriptorSector * kXdvdfsSectorSize);
    const u64 raw_desc = kXdvdfsPartitionStartRaw + (kXdvdfsDescriptorSector * kXdvdfsSectorSize);

    if (CheckMagicAt(*source, trimmed_desc, kXdvdfsMagic)) {
        variant = MediaType::XisoTrimmed;
        partition_base = kXdvdfsPartitionStartTrimmed;
        desc_offset = trimmed_desc;
    } else if (CheckMagicAt(*source, raw_desc, kXdvdfsMagic)) {
        variant = MediaType::XisoRaw;
        partition_base = kXdvdfsPartitionStartRaw;
        desc_offset = raw_desc;
    } else {
        return Error{ErrorCode::UnknownFormat, "No valid XDVDFS volume signature found"};
    }

    if (!RangeInBoundsU64(desc_offset, kXdvdfsSectorSize, source->size())) {
        return Error{ErrorCode::TruncatedData,
                     "Source truncated before XDVDFS volume descriptor sector end", desc_offset};
    }

    BinaryReader reader(*source);
    auto seek_res = reader.Seek(desc_offset);
    if (!seek_res) {
        return seek_res.error();
    }

    auto magic_res = reader.ReadFixedString(kXdvdfsMagic.size());
    if (!magic_res || *magic_res != kXdvdfsMagic) {
        return Error{ErrorCode::InvalidMagic, "Invalid volume descriptor magic"};
    }

    auto root_sec_res = reader.ReadU32LE();
    if (!root_sec_res) {
        return root_sec_res.error();
    }
    u32 root_sec = *root_sec_res;

    auto root_size_res = reader.ReadU32LE();
    if (!root_size_res) {
        return root_size_res.error();
    }
    u32 root_size = *root_size_res;

    auto ts_res = reader.ReadU64LE();
    if (!ts_res) {
        return ts_res.error();
    }
    u64 timestamp = *ts_res;

    bool valid_footer =
        CheckMagicAt(*source, desc_offset + kXdvdfsDescriptorFooterOffset, kXdvdfsMagic);

    if (root_size == 0) {
        return Error{ErrorCode::InvalidField, "Root directory size in descriptor is zero"};
    }
    if (root_size > budget.max_dir_size_bytes) {
        return Error{ErrorCode::LimitReached, "Root directory size exceeds budget"};
    }

    u64 root_sec_bytes = 0;
    if (!CheckedMulU64(static_cast<u64>(root_sec), kXdvdfsSectorSize, root_sec_bytes)) {
        return Error{ErrorCode::IntegerOverflow, "Root directory sector math overflowed"};
    }
    u64 root_offset = 0;
    if (!CheckedAddU64(partition_base, root_sec_bytes, root_offset)) {
        return Error{ErrorCode::IntegerOverflow, "Root directory offset calculation overflowed"};
    }

    if (!RangeInBoundsU64(root_offset, root_size, source->size())) {
        return Error{ErrorCode::OutOfBounds, "Root directory range exceeds source boundaries",
                     root_offset};
    }

    XdvdfsVolumeInfo info{};
    info.variant = variant;
    info.partition_base_offset = partition_base;
    info.descriptor_offset = desc_offset;
    info.root_dir_sector = root_sec;
    info.root_dir_size = root_size;
    info.creation_timestamp = timestamp;
    info.valid_footer_magic = valid_footer;

    return std::shared_ptr<XdvdfsVolume>(new XdvdfsVolume(std::move(source), info, budget));
}

Result<std::vector<XdvdfsEntry>> XdvdfsVolume::ParseDirectoryTable(u32 starting_sector,
                                                                   u32 dir_size_bytes) const {
    return ParseDirectoryTableEntries(*source_, info_.partition_base_offset, starting_sector,
                                      dir_size_bytes, budget_);
}

Result<std::vector<XdvdfsEntry>> XdvdfsVolume::ListDirectory(std::string_view path) const {
    auto comp_res = SplitAndValidateXboxPath(path);
    if (!comp_res) {
        return comp_res.error();
    }

    const auto& comps = *comp_res;
    if (comps.empty()) {
        return ParseDirectoryTable(info_.root_dir_sector, info_.root_dir_size);
    }

    u32 curr_sector = info_.root_dir_sector;
    u32 curr_size = info_.root_dir_size;

    for (size_t i = 0; i < comps.size(); ++i) {
        auto entries_res = ParseDirectoryTable(curr_sector, curr_size);
        if (!entries_res) {
            return entries_res.error();
        }

        const auto& entries = *entries_res;
        auto it = std::find_if(entries.begin(), entries.end(), [&](const XdvdfsEntry& e) {
            return EqualsIgnoreCaseAscii(e.name, comps[i]);
        });

        if (it == entries.end()) {
            return Error{ErrorCode::FileNotFound, "Directory component not found: " + comps[i]};
        }

        if (!it->is_directory) {
            return Error{ErrorCode::NotADirectory, "Component is not a directory: " + comps[i]};
        }

        curr_sector = it->starting_sector;
        curr_size = it->file_size;
    }

    return ParseDirectoryTable(curr_sector, curr_size);
}

Result<XdvdfsEntry> XdvdfsVolume::FindEntry(std::string_view path) const {
    auto comp_res = SplitAndValidateXboxPath(path);
    if (!comp_res) {
        return comp_res.error();
    }

    const auto& comps = *comp_res;
    if (comps.empty()) {
        XdvdfsEntry root{};
        root.name = "";
        root.attributes = static_cast<u8>(XdvdfsFileAttribute::Directory);
        root.is_directory = true;
        root.starting_sector = info_.root_dir_sector;
        root.file_size = info_.root_dir_size;
        root.offset_in_image = info_.partition_base_offset +
                               (static_cast<u64>(info_.root_dir_sector) * kXdvdfsSectorSize);
        return root;
    }

    u32 curr_sector = info_.root_dir_sector;
    u32 curr_size = info_.root_dir_size;

    for (size_t i = 0; i < comps.size(); ++i) {
        auto entries_res = ParseDirectoryTable(curr_sector, curr_size);
        if (!entries_res) {
            return entries_res.error();
        }

        const auto& entries = *entries_res;
        auto it = std::find_if(entries.begin(), entries.end(), [&](const XdvdfsEntry& e) {
            return EqualsIgnoreCaseAscii(e.name, comps[i]);
        });

        if (it == entries.end()) {
            return Error{ErrorCode::FileNotFound, "Entry not found: " + comps[i]};
        }

        if (i == comps.size() - 1) {
            return *it;
        }

        if (!it->is_directory) {
            return Error{ErrorCode::NotADirectory, "Component is not a directory: " + comps[i]};
        }

        curr_sector = it->starting_sector;
        curr_size = it->file_size;
    }

    return Error{ErrorCode::FileNotFound, "Entry lookup failed"};
}

Result<std::shared_ptr<SubrangeByteSource>> XdvdfsVolume::OpenFile(std::string_view path) const {
    auto entry_res = FindEntry(path);
    if (!entry_res) {
        return entry_res.error();
    }
    return OpenFile(*entry_res);
}

Result<std::shared_ptr<SubrangeByteSource>> XdvdfsVolume::OpenFile(const XdvdfsEntry& entry) const {
    if (entry.is_directory) {
        return Error{ErrorCode::IsADirectory, "Cannot open directory as a file"};
    }

    if (!RangeInBoundsU64(entry.offset_in_image, entry.file_size, source_->size())) {
        return Error{ErrorCode::TruncatedData,
                     "File data extends beyond image boundary (truncated sectors)",
                     entry.offset_in_image};
    }

    return SubrangeByteSource::Create(source_, entry.offset_in_image, entry.file_size);
}

Result<size_t> XdvdfsVolume::ReadFileAt(const XdvdfsEntry& entry, u64 offset,
                                        std::span<u8> dst) const {
    if (entry.is_directory) {
        return Error{ErrorCode::IsADirectory, "Cannot read directory as file"};
    }

    if (dst.empty()) {
        return size_t{0};
    }

    if (offset >= entry.file_size) {
        return size_t{0}; // EOF reached
    }

    u64 remaining = static_cast<u64>(entry.file_size) - offset;
    size_t to_read = static_cast<size_t>(std::min(static_cast<u64>(dst.size()), remaining));

    if (!RangeInBoundsU64(entry.offset_in_image + offset, to_read, source_->size())) {
        return Error{ErrorCode::TruncatedData, "Image truncated during file read"};
    }

    auto read_res = source_->ReadAt(entry.offset_in_image + offset, dst.subspan(0, to_read));
    if (!read_res) {
        return read_res.error();
    }

    return to_read;
}

Result<std::vector<std::pair<std::string, XdvdfsEntry>>>
XdvdfsVolume::TraverseAll(std::string_view start_path) const {
    std::vector<std::pair<std::string, XdvdfsEntry>> result;

    struct Frame {
        u32 sector;
        u32 size;
        std::string path_prefix;
        size_t depth;
    };

    std::vector<Frame> dir_stack;
    std::unordered_set<u32> visited_dirs;

    if (start_path.empty() || start_path == "/" || start_path == "\\") {
        dir_stack.push_back({info_.root_dir_sector, info_.root_dir_size, "", 1});
        visited_dirs.insert(info_.root_dir_sector);
    } else {
        auto entry_res = FindEntry(start_path);
        if (!entry_res) {
            return entry_res.error();
        }
        if (!entry_res->is_directory) {
            return Error{ErrorCode::NotADirectory, "Start path is not a directory"};
        }
        dir_stack.push_back(
            {entry_res->starting_sector, entry_res->file_size, std::string(start_path), 1});
        visited_dirs.insert(entry_res->starting_sector);
    }

    while (!dir_stack.empty()) {
        Frame frame = std::move(dir_stack.back());
        dir_stack.pop_back();

        if (frame.depth > budget_.max_depth) {
            return Error{ErrorCode::LimitReached, "Maximum directory depth exceeded"};
        }

        auto entries_res = ParseDirectoryTable(frame.sector, frame.size);
        if (!entries_res) {
            return entries_res.error();
        }

        for (auto& entry : *entries_res) {
            if (result.size() >= budget_.max_total_entries) {
                return Error{ErrorCode::LimitReached, "Total directory entries limit exceeded"};
            }

            std::string full_path =
                frame.path_prefix.empty() ? entry.name : frame.path_prefix + "\\" + entry.name;
            result.emplace_back(full_path, entry);

            if (entry.is_directory) {
                if (visited_dirs.contains(entry.starting_sector)) {
                    return Error{ErrorCode::InvalidField,
                                 "Cycle detected across directories in XDVDFS: sector revisited"};
                }
                visited_dirs.insert(entry.starting_sector);
                dir_stack.push_back(
                    {entry.starting_sector, entry.file_size, full_path, frame.depth + 1});
            }
        }
    }

    return result;
}

} // namespace xblob
