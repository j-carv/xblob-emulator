#pragma once

#include "xblob/formats/xdvdfs.hpp"
#include "xblob/vfs/vfs_volume.hpp"

#include <memory>

namespace xblob {

class XdvdfsVfsVolume final : public IVfsVolume {
public:
    explicit XdvdfsVfsVolume(std::shared_ptr<XdvdfsVolume> volume) noexcept
        : volume_(std::move(volume)) {}

    [[nodiscard]] bool IsReadOnly() const noexcept override { return true; }

    [[nodiscard]] Result<VfsFileInfo> QueryInfo(std::string_view relative_path) override {
        if (!volume_) {
            return Error{ErrorCode::InvalidState, "XDVDFS volume is null"};
        }
        auto entry_res = volume_->FindEntry(relative_path);
        if (!entry_res) {
            return entry_res.error();
        }
        VfsFileInfo info{};
        info.name = entry_res->name;
        info.size = entry_res->file_size;
        info.is_directory = entry_res->is_directory;
        info.attributes = entry_res->attributes;
        info.creation_timestamp = volume_->info().creation_timestamp;
        return info;
    }

    [[nodiscard]] Result<std::shared_ptr<const ByteSource>>
    OpenFile(std::string_view relative_path) override {
        if (!volume_) {
            return Error{ErrorCode::InvalidState, "XDVDFS volume is null"};
        }
        auto file_res = volume_->OpenFile(relative_path);
        if (!file_res) {
            return file_res.error();
        }
        return std::shared_ptr<const ByteSource>(*file_res);
    }

    [[nodiscard]] Result<std::vector<VfsDirEntry>>
    ListDirectory(std::string_view relative_path) override {
        if (!volume_) {
            return Error{ErrorCode::InvalidState, "XDVDFS volume is null"};
        }
        auto list_res = volume_->ListDirectory(relative_path);
        if (!list_res) {
            return list_res.error();
        }

        std::vector<VfsDirEntry> result;
        result.reserve(list_res->size());
        for (const auto& entry : *list_res) {
            result.push_back(VfsDirEntry{
                .name = entry.name,
                .is_directory = entry.is_directory,
                .size = entry.file_size,
                .attributes = entry.attributes,
            });
        }
        return result;
    }

    [[nodiscard]] const std::shared_ptr<XdvdfsVolume>& underlying_volume() const noexcept {
        return volume_;
    }

private:
    std::shared_ptr<XdvdfsVolume> volume_;
};

} // namespace xblob
