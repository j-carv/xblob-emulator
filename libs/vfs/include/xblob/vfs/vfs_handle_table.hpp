#pragma once

#include "xblob/common/result.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/vfs/vfs_types.hpp"

#include <memory>
#include <vector>

namespace xblob {

struct FileSlot {
    std::shared_ptr<const ByteSource> source;
    u64 position{0};
    VfsAccessMode mode{VfsAccessMode::Read};
    VfsFileInfo info{};
};

struct DirSlot {
    std::vector<VfsDirEntry> entries;
    size_t enumeration_index{0};
    VfsFileInfo info{};
};

struct HandleSlot {
    u16 generation{1};
    bool is_open{false};
    VfsNodeType type{VfsNodeType::File};
    FileSlot file{};
    DirSlot dir{};
};

class VfsHandleTable {
public:
    explicit VfsHandleTable(size_t max_handles = 4096) noexcept : max_handles_(max_handles) {}

    [[nodiscard]] Result<VfsHandle> AllocateFile(std::shared_ptr<const ByteSource> source,
                                                 VfsAccessMode mode, VfsFileInfo info);

    [[nodiscard]] Result<VfsHandle> AllocateDir(std::vector<VfsDirEntry> entries, VfsFileInfo info);

    [[nodiscard]] Result<void> Close(VfsHandle handle);

    [[nodiscard]] Result<FileSlot*> GetFile(VfsHandle handle);
    [[nodiscard]] Result<const FileSlot*> GetFile(VfsHandle handle) const;

    [[nodiscard]] Result<DirSlot*> GetDir(VfsHandle handle);
    [[nodiscard]] Result<const DirSlot*> GetDir(VfsHandle handle) const;

    [[nodiscard]] Result<VfsNodeType> GetType(VfsHandle handle) const;

    void Reset();

    [[nodiscard]] size_t OpenCount() const noexcept;

private:
    [[nodiscard]] Result<size_t> AllocateSlot();

    size_t max_handles_{4096};
    std::vector<HandleSlot> slots_;
};

} // namespace xblob
