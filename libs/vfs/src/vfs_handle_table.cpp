#include "xblob/vfs/vfs_handle_table.hpp"

namespace xblob {

Result<size_t> VfsHandleTable::AllocateSlot() {
    for (size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].is_open) {
            return i;
        }
    }

    if (slots_.size() >= max_handles_) {
        return Error{ErrorCode::LimitReached, "Maximum open VFS handle budget exceeded"};
    }

    size_t idx = slots_.size();
    slots_.push_back(HandleSlot{.generation = 1, .is_open = false});
    return idx;
}

Result<VfsHandle> VfsHandleTable::AllocateFile(std::shared_ptr<const ByteSource> source,
                                               VfsAccessMode mode, VfsFileInfo info) {
    if (!source) {
        return Error{ErrorCode::InvalidArgument, "ByteSource cannot be null"};
    }

    auto slot_res = AllocateSlot();
    if (!slot_res) {
        return slot_res.error();
    }

    size_t idx = *slot_res;
    auto& slot = slots_[idx];
    slot.is_open = true;
    slot.type = VfsNodeType::File;
    slot.file.source = std::move(source);
    slot.file.position = 0;
    slot.file.mode = mode;
    slot.file.info = std::move(info);
    slot.dir = {};

    return VfsHandle{
        .index = static_cast<u16>(idx),
        .generation = slot.generation,
    };
}

Result<VfsHandle> VfsHandleTable::AllocateDir(std::vector<VfsDirEntry> entries, VfsFileInfo info) {
    auto slot_res = AllocateSlot();
    if (!slot_res) {
        return slot_res.error();
    }

    size_t idx = *slot_res;
    auto& slot = slots_[idx];
    slot.is_open = true;
    slot.type = VfsNodeType::Directory;
    slot.dir.entries = std::move(entries);
    slot.dir.enumeration_index = 0;
    slot.dir.info = std::move(info);
    slot.file = {};

    return VfsHandle{
        .index = static_cast<u16>(idx),
        .generation = slot.generation,
    };
}

Result<void> VfsHandleTable::Close(VfsHandle handle) {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or already closed"};
    }

    slot.is_open = false;
    slot.generation = static_cast<u16>(slot.generation + 1);
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.file = {};
    slot.dir = {};

    return Result<void>::Ok();
}

Result<FileSlot*> VfsHandleTable::GetFile(VfsHandle handle) {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or closed"};
    }

    if (slot.type != VfsNodeType::File) {
        return Error{ErrorCode::IsADirectory, "Handle refers to a directory, not a file"};
    }

    return &slot.file;
}

Result<const FileSlot*> VfsHandleTable::GetFile(VfsHandle handle) const {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    const auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or closed"};
    }

    if (slot.type != VfsNodeType::File) {
        return Error{ErrorCode::IsADirectory, "Handle refers to a directory, not a file"};
    }

    return &slot.file;
}

Result<DirSlot*> VfsHandleTable::GetDir(VfsHandle handle) {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or closed"};
    }

    if (slot.type != VfsNodeType::Directory) {
        return Error{ErrorCode::NotADirectory, "Handle refers to a file, not a directory"};
    }

    return &slot.dir;
}

Result<const DirSlot*> VfsHandleTable::GetDir(VfsHandle handle) const {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    const auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or closed"};
    }

    if (slot.type != VfsNodeType::Directory) {
        return Error{ErrorCode::NotADirectory, "Handle refers to a file, not a directory"};
    }

    return &slot.dir;
}

Result<VfsNodeType> VfsHandleTable::GetType(VfsHandle handle) const {
    if (!handle.IsValid() || handle.index >= slots_.size()) {
        return Error{ErrorCode::InvalidHandle, "Invalid handle index"};
    }

    const auto& slot = slots_[handle.index];
    if (!slot.is_open || slot.generation != handle.generation) {
        return Error{ErrorCode::InvalidHandle, "Handle is stale or closed"};
    }

    return slot.type;
}

void VfsHandleTable::Reset() {
    for (auto& slot : slots_) {
        if (slot.is_open) {
            slot.is_open = false;
            slot.generation = static_cast<u16>(slot.generation + 1);
            if (slot.generation == 0) {
                slot.generation = 1;
            }
            slot.file = {};
            slot.dir = {};
        }
    }
}

size_t VfsHandleTable::OpenCount() const noexcept {
    size_t count = 0;
    for (const auto& slot : slots_) {
        if (slot.is_open) {
            count++;
        }
    }
    return count;
}

} // namespace xblob
