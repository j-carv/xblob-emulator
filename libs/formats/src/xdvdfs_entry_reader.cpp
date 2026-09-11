#include "xdvdfs_entry_reader.hpp"

#include "xblob/common/safe_math.hpp"

#include <array>
#include <unordered_set>

namespace xblob {

bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb + ('a' - 'A'));
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

std::string ToLowerAscii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c >= 'A' && c <= 'Z') {
            result.push_back(static_cast<char>(c + ('a' - 'A')));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

Result<std::vector<std::string>> SplitAndValidateXboxPath(std::string_view path) {
    std::vector<std::string> components;

    // Strip drive prefix if present (e.g., "D:" or "d:")
    if (path.size() >= 2 && (path[1] == ':')) {
        char drive = path[0];
        if ((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z')) {
            path.remove_prefix(2);
        } else {
            return Error{ErrorCode::InvalidPath, "Invalid drive prefix in path"};
        }
    }

    // Trim leading slashes
    while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
        path.remove_prefix(1);
    }

    // Trim trailing slashes
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
        path.remove_suffix(1);
    }

    if (path.empty()) {
        return components; // Root directory
    }

    size_t start = 0;
    while (start < path.size()) {
        size_t next_sep = path.find_first_of("/\\", start);
        if (next_sep == std::string_view::npos) {
            next_sep = path.size();
        }

        std::string_view comp = path.substr(start, next_sep - start);
        if (comp.empty()) {
            return Error{ErrorCode::InvalidPath, "Empty component in path (consecutive slashes)"};
        }

        if (comp == "." || comp == "..") {
            return Error{ErrorCode::InvalidPath, "Path traversal component is forbidden"};
        }

        for (char c : comp) {
            auto uc = static_cast<unsigned char>(c);
            if (uc < 32 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
                c == '|') {
                return Error{ErrorCode::InvalidPath, "Invalid character in path component"};
            }
        }

        components.emplace_back(comp);
        start = next_sep + 1;
    }

    return components;
}

Result<std::vector<XdvdfsEntry>> ParseDirectoryTableEntries(const ByteSource& source,
                                                            u64 partition_base, u32 starting_sector,
                                                            u32 dir_size_bytes,
                                                            const XdvdfsBudgetConfig& budget) {
    if (dir_size_bytes == 0) {
        return Error{ErrorCode::InvalidField, "Directory table size cannot be zero"};
    }
    if (dir_size_bytes > budget.max_dir_size_bytes) {
        return Error{ErrorCode::LimitReached, "Directory table size exceeds budget"};
    }

    u64 sec_offset = 0;
    if (!CheckedMulU64(static_cast<u64>(starting_sector), kXdvdfsSectorSize, sec_offset)) {
        return Error{ErrorCode::IntegerOverflow, "Directory sector math overflowed"};
    }
    u64 dir_base_offset = 0;
    if (!CheckedAddU64(partition_base, sec_offset, dir_base_offset)) {
        return Error{ErrorCode::IntegerOverflow, "Directory base offset overflowed"};
    }

    if (!RangeInBoundsU64(dir_base_offset, dir_size_bytes, source.size())) {
        return Error{ErrorCode::OutOfBounds, "Directory table exceeds image bounds",
                     dir_base_offset};
    }

    struct WorkItem {
        u16 dword_offset;
        size_t depth;
    };

    std::vector<WorkItem> stack;
    stack.push_back({0, 1});

    std::unordered_set<u16> visited_offsets;
    std::unordered_set<std::string> seen_names;
    std::vector<XdvdfsEntry> entries;

    while (!stack.empty()) {
        WorkItem item = stack.back();
        stack.pop_back();

        if (item.depth > budget.max_depth) {
            return Error{ErrorCode::LimitReached, "Directory BST depth budget exceeded"};
        }

        if (visited_offsets.contains(item.dword_offset)) {
            return Error{ErrorCode::InvalidField,
                         "Cycle detected in XDVDFS directory tree (node revisited)"};
        }
        visited_offsets.insert(item.dword_offset);

        if (visited_offsets.size() > budget.max_entries_per_dir) {
            return Error{ErrorCode::LimitReached, "Directory entry count exceeds budget"};
        }

        u64 byte_offset = static_cast<u64>(item.dword_offset) * 4;
        if (byte_offset + 14 > dir_size_bytes) {
            return Error{ErrorCode::OutOfBounds,
                         "Directory entry header exceeds directory table size"};
        }

        std::array<u8, 14> hdr{};
        auto read_res = source.ReadAt(dir_base_offset + byte_offset, hdr);
        if (!read_res) {
            return read_res.error();
        }

        u16 left_child = static_cast<u16>(static_cast<u16>(hdr[0]) |
                                          static_cast<u16>(static_cast<u16>(hdr[1]) << 8));
        u16 right_child = static_cast<u16>(static_cast<u16>(hdr[2]) |
                                           static_cast<u16>(static_cast<u16>(hdr[3]) << 8));
        u32 entry_sector = static_cast<u32>(hdr[4]) | (static_cast<u32>(hdr[5]) << 8) |
                           (static_cast<u32>(hdr[6]) << 16) | (static_cast<u32>(hdr[7]) << 24);
        u32 file_size = static_cast<u32>(hdr[8]) | (static_cast<u32>(hdr[9]) << 8) |
                        (static_cast<u32>(hdr[10]) << 16) | (static_cast<u32>(hdr[11]) << 24);
        u8 attributes = hdr[12];
        u8 name_len = hdr[13];

        if (name_len == 0) {
            return Error{ErrorCode::InvalidField, "XDVDFS directory entry filename length is zero"};
        }
        if (name_len > budget.max_filename_length) {
            return Error{ErrorCode::LimitReached, "Filename length exceeds budget"};
        }
        if (byte_offset + 14 + name_len > dir_size_bytes) {
            return Error{ErrorCode::OutOfBounds,
                         "XDVDFS directory entry filename exceeds directory table bounds"};
        }

        std::vector<u8> name_buf(name_len);
        auto name_res = source.ReadAt(dir_base_offset + byte_offset + 14, name_buf);
        if (!name_res) {
            return name_res.error();
        }

        for (u8 ch : name_buf) {
            if (ch == 0 || ch == '/' || ch == '\\' || ch < 32) {
                return Error{ErrorCode::InvalidPath,
                             "Filename in directory entry contains forbidden characters"};
            }
        }

        std::string name(name_buf.begin(), name_buf.end());
        std::string lower = ToLowerAscii(name);
        if (seen_names.contains(lower)) {
            return Error{ErrorCode::InvalidField, "Duplicate filename in directory table: " + name};
        }
        seen_names.insert(std::move(lower));

        u64 entry_sec_offset = 0;
        if (!CheckedMulU64(static_cast<u64>(entry_sector), kXdvdfsSectorSize, entry_sec_offset)) {
            return Error{ErrorCode::IntegerOverflow, "Entry sector math overflowed"};
        }
        u64 entry_img_offset = 0;
        if (!CheckedAddU64(partition_base, entry_sec_offset, entry_img_offset)) {
            return Error{ErrorCode::IntegerOverflow, "Entry image offset overflowed"};
        }

        XdvdfsEntry entry{};
        entry.name = std::move(name);
        entry.attributes = attributes;
        entry.is_directory = IsDirectoryAttribute(attributes);
        entry.starting_sector = entry_sector;
        entry.file_size = file_size;
        entry.offset_in_image = entry_img_offset;
        entries.push_back(std::move(entry));

        if (right_child != 0 && right_child != 0xFFFF) {
            u64 right_byte_offset = static_cast<u64>(right_child) * 4;
            if (right_byte_offset + 14 > dir_size_bytes) {
                return Error{ErrorCode::OutOfBounds,
                             "Right child pointer points outside directory table"};
            }
            stack.push_back({right_child, item.depth + 1});
        }

        if (left_child != 0 && left_child != 0xFFFF) {
            u64 left_byte_offset = static_cast<u64>(left_child) * 4;
            if (left_byte_offset + 14 > dir_size_bytes) {
                return Error{ErrorCode::OutOfBounds,
                             "Left child pointer points outside directory table"};
            }
            stack.push_back({left_child, item.depth + 1});
        }
    }

    return entries;
}

} // namespace xblob
