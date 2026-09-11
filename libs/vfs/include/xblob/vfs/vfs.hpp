#pragma once

#include "xblob/common/result.hpp"
#include "xblob/vfs/vfs_handle_table.hpp"
#include "xblob/vfs/vfs_types.hpp"
#include "xblob/vfs/vfs_volume.hpp"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xblob {

class Vfs {
public:
    Vfs() = default;
    ~Vfs() = default;

    Vfs(const Vfs&) = delete;
    Vfs& operator=(const Vfs&) = delete;
    Vfs(Vfs&&) noexcept = default;
    Vfs& operator=(Vfs&&) noexcept = default;

    [[nodiscard]] Result<void> Mount(std::string_view alias, std::shared_ptr<IVfsVolume> volume);
    [[nodiscard]] Result<void> Unmount(std::string_view alias);
    [[nodiscard]] bool IsMounted(std::string_view alias) const noexcept;
    [[nodiscard]] std::shared_ptr<IVfsVolume> GetMountedVolume(std::string_view alias) const;

    [[nodiscard]] Result<VfsHandle> OpenFile(std::string_view path,
                                             VfsAccessMode mode = VfsAccessMode::Read);
    [[nodiscard]] Result<VfsHandle> OpenDirectory(std::string_view path);
    [[nodiscard]] Result<void> Close(VfsHandle handle);

    [[nodiscard]] Result<size_t> Read(VfsHandle handle, std::span<u8> dst);
    [[nodiscard]] Result<u64> Seek(VfsHandle handle, i64 offset, VfsSeekOrigin origin);
    [[nodiscard]] Result<u64> GetPosition(VfsHandle handle) const;

    [[nodiscard]] Result<VfsFileInfo> QueryByHandle(VfsHandle handle) const;
    [[nodiscard]] Result<VfsFileInfo> QueryPath(std::string_view path);

    [[nodiscard]] Result<void> EnumerateDirectory(VfsHandle handle,
                                                  std::vector<VfsDirEntry>& out_entries);

    // Rejection of mutating operations on read-only VFS
    [[nodiscard]] Result<VfsHandle> CreateFile(std::string_view path);
    [[nodiscard]] Result<size_t> Write(VfsHandle handle, std::span<const u8> src);
    [[nodiscard]] Result<void> DeleteFile(std::string_view path);

    void Reset();

    [[nodiscard]] const VfsHandleTable& handle_table() const noexcept { return handle_table_; }

private:
    [[nodiscard]] Result<std::shared_ptr<IVfsVolume>> ResolveVolume(std::string_view alias) const;

    std::unordered_map<std::string, std::shared_ptr<IVfsVolume>> mounts_;
    VfsHandleTable handle_table_;
};

} // namespace xblob
