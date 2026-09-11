#include "tests/fixtures/synthetic_xdvdfs.hpp"

#include "xblob/common/safe_math.hpp"
#include "xblob/formats/xiso.hpp"

#include <algorithm>
#include <cstring>
#include <map>

namespace xblob::testing {

namespace {

void WriteU16(std::vector<u8>& buf, size_t offset, u16 val) {
    buf[offset] = static_cast<u8>(val & 0xFF);
    buf[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
}

void WriteU32(std::vector<u8>& buf, size_t offset, u32 val) {
    buf[offset] = static_cast<u8>(val & 0xFF);
    buf[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
    buf[offset + 2] = static_cast<u8>((val >> 16) & 0xFF);
    buf[offset + 3] = static_cast<u8>((val >> 24) & 0xFF);
}

void WriteU64(std::vector<u8>& buf, size_t offset, u64 val) {
    for (size_t i = 0; i < 8; ++i) {
        buf[offset + i] = static_cast<u8>((val >> (i * 8)) & 0xFF);
    }
}

struct Node {
    std::string name;
    bool is_dir{false};
    std::vector<u8> file_data;
    std::map<std::string, std::shared_ptr<Node>> children;
    u32 sector{0};
    u32 size{0};
};

u16 EmitBst(const std::vector<std::shared_ptr<Node>>& list, int left, int right,
            std::vector<u8>& table) {
    if (left > right) {
        return 0;
    }

    int mid = left + (right - left) / 2;
    const auto& node = list[static_cast<size_t>(mid)];

    size_t entry_offset = table.size();
    u8 name_len = static_cast<u8>(node->name.size());
    size_t raw_len = 14 + static_cast<size_t>(name_len);
    size_t aligned_len = (raw_len + 3U) & ~3U;

    table.resize(entry_offset + aligned_len, 0);

    u8 attrs = node->is_dir ? static_cast<u8>(XdvdfsFileAttribute::Directory)
                            : static_cast<u8>(XdvdfsFileAttribute::Normal);

    WriteU32(table, entry_offset + 4, node->sector);
    WriteU32(table, entry_offset + 8, node->size);
    table[entry_offset + 12] = attrs;
    table[entry_offset + 13] = name_len;
    std::memcpy(&table[entry_offset + 14], node->name.data(), name_len);

    u16 left_dword = EmitBst(list, left, mid - 1, table);
    u16 right_dword = EmitBst(list, mid + 1, right, table);

    WriteU16(table, entry_offset + 0, left_dword);
    WriteU16(table, entry_offset + 2, right_dword);

    return static_cast<u16>(entry_offset / 4);
}

class SyntheticRawByteSource final : public ByteSource {
public:
    explicit SyntheticRawByteSource(std::vector<u8> partition_data) noexcept
        : partition_data_(std::move(partition_data)) {}

    [[nodiscard]] u64 size() const noexcept override {
        return kXdvdfsPartitionStartRaw + partition_data_.size();
    }

    [[nodiscard]] Result<void> ReadAt(u64 offset, std::span<u8> dst) const override {
        if (!RangeInBoundsU64(offset, dst.size(), size())) {
            return Error{ErrorCode::OutOfBounds, "Read out of bounds in raw source", offset};
        }

        if (dst.empty()) {
            return Result<void>::Ok();
        }

        u64 cur_offset = offset;
        size_t dst_pos = 0;
        size_t remaining = dst.size();

        if (cur_offset < kXdvdfsPartitionStartRaw) {
            size_t lead_len = static_cast<size_t>(
                std::min(static_cast<u64>(remaining), kXdvdfsPartitionStartRaw - cur_offset));
            std::fill(dst.data(), dst.data() + lead_len, 0);
            dst_pos += lead_len;
            remaining -= lead_len;
            cur_offset += lead_len;
        }

        if (remaining > 0) {
            u64 part_offset = cur_offset - kXdvdfsPartitionStartRaw;
            std::copy(partition_data_.data() + part_offset,
                      partition_data_.data() + part_offset + remaining, dst.data() + dst_pos);
        }

        return Result<void>::Ok();
    }

    [[nodiscard]] Result<ByteSpan> SpanAt(u64 offset, size_t count) const override {
        if (!RangeInBoundsU64(offset, count, size())) {
            return Error{ErrorCode::OutOfBounds, "Span out of bounds in raw source", offset};
        }
        if (offset >= kXdvdfsPartitionStartRaw) {
            u64 part_offset = offset - kXdvdfsPartitionStartRaw;
            return ByteSpan(partition_data_.data() + part_offset, count);
        }
        return Error{ErrorCode::UnsupportedFormat,
                     "SpanAt across lead-in/partition boundary unsupported"};
    }

private:
    std::vector<u8> partition_data_;
};

} // namespace

std::vector<u8> BuildValidTrimmedXdvdfsImage(const std::vector<SyntheticFileEntry>& files) {
    auto root = std::make_shared<Node>();
    root->name = "";
    root->is_dir = true;

    for (const auto& f : files) {
        std::string_view p = f.path;
        while (!p.empty() && (p.front() == '/' || p.front() == '\\')) {
            p.remove_prefix(1);
        }

        std::shared_ptr<Node> cur = root;
        size_t start = 0;
        while (start < p.size()) {
            size_t next = p.find_first_of("/\\", start);
            if (next == std::string_view::npos) {
                next = p.size();
            }
            std::string part(p.substr(start, next - start));
            if (next == p.size()) {
                auto file_node = std::make_shared<Node>();
                file_node->name = part;
                file_node->is_dir = false;
                file_node->file_data = f.data;
                file_node->size = static_cast<u32>(f.data.size());
                cur->children[part] = file_node;
            } else {
                auto it = cur->children.find(part);
                if (it == cur->children.end()) {
                    auto dir_node = std::make_shared<Node>();
                    dir_node->name = part;
                    dir_node->is_dir = true;
                    cur->children[part] = dir_node;
                    cur = dir_node;
                } else {
                    cur = it->second;
                }
            }
            start = next + 1;
        }
    }

    std::vector<std::shared_ptr<Node>> all_dirs;
    std::vector<std::shared_ptr<Node>> all_files;

    auto collect = [&](auto& self, const std::shared_ptr<Node>& n) -> void {
        if (n->is_dir) {
            all_dirs.push_back(n);
            for (auto& [_, child] : n->children) {
                self(self, child);
            }
        } else {
            all_files.push_back(n);
        }
    };
    collect(collect, root);

    u32 current_sector = 33;

    for (auto& d : all_dirs) {
        d->sector = current_sector;
        d->size = 2048;
        current_sector += 1;
    }

    for (auto& f : all_files) {
        f->sector = current_sector;
        u32 sectors_needed = (f->size + 2047) / 2048;
        if (sectors_needed == 0) {
            sectors_needed = 1;
        }
        current_sector += sectors_needed;
    }

    std::vector<u8> image(static_cast<size_t>(current_sector) * 2048, 0);

    const size_t vd_offset = 32 * 2048;
    std::memcpy(&image[vd_offset], kXdvdfsMagic.data(), kXdvdfsMagic.size());
    WriteU32(image, vd_offset + 20, root->sector);
    WriteU32(image, vd_offset + 24, root->size);
    WriteU64(image, vd_offset + 28, 0x01D8ABCD12345678ULL);
    std::memcpy(&image[vd_offset + kXdvdfsDescriptorFooterOffset], kXdvdfsMagic.data(),
                kXdvdfsMagic.size());

    for (auto& d : all_dirs) {
        std::vector<std::shared_ptr<Node>> sorted_children;
        for (auto& [_, child] : d->children) {
            sorted_children.push_back(child);
        }
        std::sort(sorted_children.begin(), sorted_children.end(),
                  [](const auto& a, const auto& b) { return a->name < b->name; });

        std::vector<u8> table;
        EmitBst(sorted_children, 0, static_cast<int>(sorted_children.size()) - 1, table);
        table.resize(2048, 0xFF);

        size_t dir_offset = static_cast<size_t>(d->sector) * 2048;
        std::memcpy(&image[dir_offset], table.data(), 2048);
    }

    for (auto& f : all_files) {
        if (!f->file_data.empty()) {
            size_t file_offset = static_cast<size_t>(f->sector) * 2048;
            std::memcpy(&image[file_offset], f->file_data.data(), f->file_data.size());
        }
    }

    return image;
}

std::shared_ptr<ByteSource>
CreateSyntheticRawXdvdfsSource(const std::vector<SyntheticFileEntry>& files) {
    std::vector<u8> part_data = BuildValidTrimmedXdvdfsImage(files);
    return std::make_shared<SyntheticRawByteSource>(std::move(part_data));
}

std::vector<u8> CreateXdvdfsWithBstCycle() {
    auto img = BuildValidTrimmedXdvdfsImage(
        {{"DEFAULT.XBE", {'X', 'B', 'E', '1'}}, {"FILE2.BIN", {'1', '2', '3'}}});
    size_t root_offset = 33 * 2048;
    u16 left = static_cast<u16>(img[root_offset + 0] | (img[root_offset + 1] << 8));
    u16 right = static_cast<u16>(img[root_offset + 2] | (img[root_offset + 3] << 8));
    u16 child = (right != 0 && right != 0xFFFF) ? right : left;
    if (child != 0 && child != 0xFFFF) {
        size_t child_off = root_offset + static_cast<size_t>(child) * 4;
        WriteU16(img, child_off + 2, child); // Points to itself!
    } else {
        WriteU16(img, root_offset + 2, 8);
        size_t child_off = root_offset + 32;
        WriteU16(img, child_off + 2, 8);
    }
    return img;
}

std::vector<u8> CreateXdvdfsWithCrossDirectoryCycle() {
    auto img = BuildValidTrimmedXdvdfsImage({{"DIR/TEST.TXT", {'H', 'I'}}});
    size_t dir_offset = 34 * 2048;
    WriteU32(img, dir_offset + 4, 33);
    img[dir_offset + 12] = static_cast<u8>(XdvdfsFileAttribute::Directory);
    return img;
}

std::vector<u8> CreateXdvdfsWithDuplicateEntries() {
    auto img = BuildValidTrimmedXdvdfsImage({{"DEFAULT.XBE", {'1'}}});
    size_t root_offset = 33 * 2048;
    WriteU16(img, root_offset + 2, 8); // Right child at DWORD 8 (byte 32)
    size_t e2 = root_offset + 32;
    WriteU16(img, e2 + 0, 0);
    WriteU16(img, e2 + 2, 0);
    WriteU32(img, e2 + 4, 34);
    WriteU32(img, e2 + 8, 1);
    img[e2 + 12] = static_cast<u8>(XdvdfsFileAttribute::Normal);
    img[e2 + 13] = 11; // length of "default.xbe"
    std::memcpy(&img[e2 + 14], "default.xbe", 11);
    return img;
}

std::vector<u8> CreateXdvdfsWithInvalidFilenameChars() {
    auto img = BuildValidTrimmedXdvdfsImage({{"GOOD.BIN", {'1'}}});
    size_t root_offset = 33 * 2048;
    img[root_offset + 14] = '/';
    return img;
}

std::vector<u8> CreateXdvdfsWithTruncatedFileSector() {
    auto img = BuildValidTrimmedXdvdfsImage({{"TRUNC.BIN", {'A', 'B'}}});
    size_t root_offset = 33 * 2048;
    WriteU32(img, root_offset + 4, 99999);
    return img;
}

std::vector<u8> CreateXdvdfsWithDeepBst(size_t depth) {
    std::vector<u8> img(35 * 2048, 0);
    const size_t vd_offset = 32 * 2048;
    std::memcpy(&img[vd_offset], kXdvdfsMagic.data(), kXdvdfsMagic.size());
    WriteU32(img, vd_offset + 20, 33);
    WriteU32(img, vd_offset + 24, 2048);
    std::memcpy(&img[vd_offset + kXdvdfsDescriptorFooterOffset], kXdvdfsMagic.data(),
                kXdvdfsMagic.size());

    size_t root_offset = 33 * 2048;
    for (size_t i = 0; i < depth; ++i) {
        size_t off = root_offset + i * 32;
        if (off + 32 > root_offset + 2048) {
            break;
        }
        u16 next_dword = (i + 1 < depth) ? static_cast<u16>((off - root_offset + 32) / 4) : 0;
        WriteU16(img, off + 0, 0);
        WriteU16(img, off + 2, next_dword);
        WriteU32(img, off + 4, 34);
        WriteU32(img, off + 8, 4);
        img[off + 12] = static_cast<u8>(XdvdfsFileAttribute::Normal);
        std::string name = "F" + std::to_string(i);
        img[off + 13] = static_cast<u8>(name.size());
        std::memcpy(&img[off + 14], name.data(), name.size());
    }
    return img;
}

} // namespace xblob::testing
