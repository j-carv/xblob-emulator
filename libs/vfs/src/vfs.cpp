#include "xblob/vfs/vfs.hpp"

#include "vfs_path.hpp"
#include "xblob/common/safe_math.hpp"

#include <algorithm>
#include <cctype>

namespace xblob {

namespace {

std::string ToUpperAscii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

} // namespace

Result<void> Vfs::Mount(std::string_view alias, std::shared_ptr<IVfsVolume> volume) {
    if (!volume) {
        return Error{ErrorCode::InvalidArgument, "Volume cannot be null"};
    }
    if (alias.empty()) {
        return Error{ErrorCode::InvalidArgument, "Mount alias cannot be empty"};
    }

    std::string key = ToUpperAscii(alias);
    if (key.size() >= 2 && key[1] == ':') {
        key = key.substr(0, 1);
    }
    mounts_[key] = std::move(volume);
    return Result<void>::Ok();
}

Result<void> Vfs::Unmount(std::string_view alias) {
    std::string key = ToUpperAscii(alias);
    if (key.size() >= 2 && key[1] == ':') {
        key = key.substr(0, 1);
    }
    auto it = mounts_.find(key);
    if (it == mounts_.end()) {
        return Error{ErrorCode::FileNotFound, "Mount alias not found"};
    }
    mounts_.erase(it);
    return Result<void>::Ok();
}

bool Vfs::IsMounted(std::string_view alias) const noexcept {
    std::string key = ToUpperAscii(alias);
    if (key.size() >= 2 && key[1] == ':') {
        key = key.substr(0, 1);
    }
    return mounts_.contains(key);
}

std::shared_ptr<IVfsVolume> Vfs::GetMountedVolume(std::string_view alias) const {
    std::string key = ToUpperAscii(alias);
    if (key.size() >= 2 && key[1] == ':') {
        key = key.substr(0, 1);
    }
    auto it = mounts_.find(key);
    if (it == mounts_.end()) {
        return nullptr;
    }
    return it->second;
}

Result<std::shared_ptr<IVfsVolume>> Vfs::ResolveVolume(std::string_view alias) const {
    std::string key = ToUpperAscii(alias);
    auto it = mounts_.find(key);
    if (it == mounts_.end()) {
        return Error{ErrorCode::FileNotFound, "Drive or volume alias not mounted: " + key};
    }
    return it->second;
}

Result<VfsHandle> Vfs::OpenFile(std::string_view path, VfsAccessMode mode) {
    if ((static_cast<u8>(mode) & static_cast<u8>(VfsAccessMode::Write)) != 0) {
        return Error{ErrorCode::ReadOnlyFileSystem, "Cannot open for write on read-only VFS"};
    }

    auto parsed_res = ParseAndNormalizeXboxPath(path);
    if (!parsed_res) {
        return parsed_res.error();
    }

    auto vol_res = ResolveVolume(parsed_res->mount_alias);
    if (!vol_res) {
        return vol_res.error();
    }
    auto vol = *vol_res;

    auto info_res = vol->QueryInfo(parsed_res->relative_path);
    if (!info_res) {
        return info_res.error();
    }

    if (info_res->is_directory) {
        return Error{ErrorCode::IsADirectory, "Target is a directory, cannot OpenFile"};
    }

    auto source_res = vol->OpenFile(parsed_res->relative_path);
    if (!source_res) {
        return source_res.error();
    }

    return handle_table_.AllocateFile(*source_res, mode, *info_res);
}

Result<VfsHandle> Vfs::OpenDirectory(std::string_view path) {
    auto parsed_res = ParseAndNormalizeXboxPath(path);
    if (!parsed_res) {
        return parsed_res.error();
    }

    auto vol_res = ResolveVolume(parsed_res->mount_alias);
    if (!vol_res) {
        return vol_res.error();
    }
    auto vol = *vol_res;

    VfsFileInfo info{};
    if (parsed_res->relative_path.empty()) {
        info.name = "\\";
        info.is_directory = true;
    } else {
        auto info_res = vol->QueryInfo(parsed_res->relative_path);
        if (!info_res) {
            return info_res.error();
        }
        if (!info_res->is_directory) {
            return Error{ErrorCode::NotADirectory, "Target is a file, cannot OpenDirectory"};
        }
        info = *info_res;
    }

    auto list_res = vol->ListDirectory(parsed_res->relative_path);
    if (!list_res) {
        return list_res.error();
    }

    return handle_table_.AllocateDir(*list_res, std::move(info));
}

Result<void> Vfs::Close(VfsHandle handle) {
    return handle_table_.Close(handle);
}

Result<size_t> Vfs::Read(VfsHandle handle, std::span<u8> dst) {
    auto file_res = handle_table_.GetFile(handle);
    if (!file_res) {
        return file_res.error();
    }

    auto* file = *file_res;
    u64 cur_pos = file->position;
    u64 file_sz = file->info.size;

    if (cur_pos >= file_sz || dst.empty()) {
        return 0ULL;
    }

    u64 available = file_sz - cur_pos;
    size_t to_read = std::min(static_cast<u64>(dst.size()), available);

    auto read_res = file->source->ReadAt(cur_pos, dst.subspan(0, to_read));
    if (!read_res) {
        return read_res.error();
    }

    // Transactional commit: position updated only on success
    file->position += to_read;
    return to_read;
}

Result<u64> Vfs::Seek(VfsHandle handle, i64 offset, VfsSeekOrigin origin) {
    auto file_res = handle_table_.GetFile(handle);
    if (!file_res) {
        return file_res.error();
    }

    auto* file = *file_res;
    i64 target = 0;

    switch (origin) {
    case VfsSeekOrigin::Begin:
        target = offset;
        break;
    case VfsSeekOrigin::Current:
        target = static_cast<i64>(file->position) + offset;
        break;
    case VfsSeekOrigin::End:
        target = static_cast<i64>(file->info.size) + offset;
        break;
    }

    if (target < 0) {
        return Error{ErrorCode::InvalidArgument, "Cannot seek before beginning of file"};
    }

    // Transactional commit: update position
    file->position = static_cast<u64>(target);
    return file->position;
}

Result<u64> Vfs::GetPosition(VfsHandle handle) const {
    auto file_res = handle_table_.GetFile(handle);
    if (!file_res) {
        return file_res.error();
    }
    return (*file_res)->position;
}

Result<VfsFileInfo> Vfs::QueryByHandle(VfsHandle handle) const {
    auto type_res = handle_table_.GetType(handle);
    if (!type_res) {
        return type_res.error();
    }

    if (*type_res == VfsNodeType::File) {
        auto file_res = handle_table_.GetFile(handle);
        if (!file_res) {
            return file_res.error();
        }
        return (*file_res)->info;
    }

    auto dir_res = handle_table_.GetDir(handle);
    if (!dir_res) {
        return dir_res.error();
    }
    return (*dir_res)->info;
}

Result<VfsFileInfo> Vfs::QueryPath(std::string_view path) {
    auto parsed_res = ParseAndNormalizeXboxPath(path);
    if (!parsed_res) {
        return parsed_res.error();
    }

    auto vol_res = ResolveVolume(parsed_res->mount_alias);
    if (!vol_res) {
        return vol_res.error();
    }
    return (*vol_res)->QueryInfo(parsed_res->relative_path);
}

Result<void> Vfs::EnumerateDirectory(VfsHandle handle, std::vector<VfsDirEntry>& out_entries) {
    auto dir_res = handle_table_.GetDir(handle);
    if (!dir_res) {
        return dir_res.error();
    }

    out_entries = (*dir_res)->entries;
    return Result<void>::Ok();
}

Result<VfsHandle> Vfs::CreateFile(std::string_view /*path*/) {
    return Error{ErrorCode::ReadOnlyFileSystem, "CreateFile is unsupported on read-only VFS"};
}

Result<size_t> Vfs::Write(VfsHandle /*handle*/, std::span<const u8> /*src*/) {
    return Error{ErrorCode::ReadOnlyFileSystem, "Write is unsupported on read-only VFS"};
}

Result<void> Vfs::DeleteFile(std::string_view /*path*/) {
    return Error{ErrorCode::ReadOnlyFileSystem, "DeleteFile is unsupported on read-only VFS"};
}

void Vfs::Reset() {
    handle_table_.Reset();
    mounts_.clear();
}

} // namespace xblob
