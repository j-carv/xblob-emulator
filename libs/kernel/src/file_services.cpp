#include "xblob/kernel/file_services.hpp"

#include <vector>

namespace xblob::kernel {

u32 MapVfsErrorToNtStatus(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Ok:
        return kNtStatusSuccess;
    case ErrorCode::FileNotFound:
        return kNtStatusObjectNameNotFound;
    case ErrorCode::InvalidPath:
        return kNtStatusObjectNameNotFound;
    case ErrorCode::InvalidHandle:
        return kNtStatusInvalidHandle;
    case ErrorCode::AccessDenied:
    case ErrorCode::ReadOnlyFileSystem:
        return kNtStatusAccessDenied;
    case ErrorCode::UnexpectedEof:
        return kNtStatusEndOfFile;
    case ErrorCode::InvalidArgument:
    case ErrorCode::OutOfBounds:
        return kNtStatusInvalidParameter;
    case ErrorCode::IsADirectory:
        return kNtStatusFileIsADirectory;
    case ErrorCode::NotADirectory:
        return kNtStatusNotADirectory;
    case ErrorCode::UnsupportedFeature:
    case ErrorCode::UnsupportedFormat:
        return kNtStatusNotImplemented;
    default:
        return kNtStatusUnsuccessful;
    }
}

namespace {

Result<std::string> ReadGuestStringBounded(GuestAddr addr, const memory::AddressSpace& mem,
                                           size_t max_len = 512) {
    std::string s;
    s.reserve(64);
    for (size_t i = 0; i < max_len; ++i) {
        auto r = mem.Read8(addr + static_cast<GuestAddr>(i));
        if (!r) {
            return r.error();
        }
        u8 val = *r;
        if (val == 0) {
            return s;
        }
        s.push_back(static_cast<char>(val));
    }
    return Error{ErrorCode::OutOfBounds, "String exceeds maximum guest path length"};
}

} // namespace

Result<u32> KernelFileServices::CreateFile(GuestAddr path_ptr, u32 desired_access, u32 disposition,
                                           GuestAddr out_handle_ptr, memory::AddressSpace& mem) {
    if (!vfs_) {
        return kNtStatusUnsuccessful;
    }

    if (out_handle_ptr != 0) {
        auto val_res = mem.ValidateRange(out_handle_ptr, 4, memory::MemoryPermission::Write);
        if (!val_res) {
            return kNtStatusInvalidParameter;
        }
    }

    auto path_res = ReadGuestStringBounded(path_ptr, mem);
    if (!path_res) {
        return kNtStatusInvalidParameter;
    }

    // Write access rejection
    if ((desired_access & 0x40000000U) != 0 || disposition == 1 /* CREATE_NEW */ ||
        disposition == 2 /* CREATE_ALWAYS */ || disposition == 4 /* OPEN_ALWAYS */) {
        return kNtStatusAccessDenied;
    }

    auto open_res = vfs_->OpenFile(*path_res, VfsAccessMode::Read);
    if (!open_res) {
        return MapVfsErrorToNtStatus(open_res.error().code);
    }

    u32 handle_val = open_res->ToU32();
    if (out_handle_ptr != 0) {
        auto w_res = mem.Write32(out_handle_ptr, handle_val);
        if (!w_res) {
            (void)vfs_->Close(*open_res);
            return kNtStatusInvalidParameter;
        }
    }

    return kNtStatusSuccess;
}

Result<u32> KernelFileServices::ReadFile(u32 handle_val, GuestAddr buffer_addr, u32 bytes_to_read,
                                         GuestAddr bytes_read_ptr, GuestAddr overlapped_ptr,
                                         memory::AddressSpace& mem) {
    // Explicit rejection of asynchronous requests without pretending completion
    if (overlapped_ptr != 0) {
        return kNtStatusNotImplemented;
    }

    if (!vfs_) {
        return kNtStatusUnsuccessful;
    }

    // Validate bytes_read_ptr if provided
    if (bytes_read_ptr != 0) {
        auto val_read_ptr = mem.ValidateRange(bytes_read_ptr, 4, memory::MemoryPermission::Write);
        if (!val_read_ptr) {
            return kNtStatusInvalidParameter;
        }
    }

    // Validate entire buffer range before any VFS reading or position changes
    if (bytes_to_read > 0) {
        auto val_buf =
            mem.ValidateRange(buffer_addr, bytes_to_read, memory::MemoryPermission::Write);
        if (!val_buf) {
            return kNtStatusInvalidParameter;
        }
    }

    VfsHandle handle = VfsHandle::FromU32(handle_val);
    if (!handle.IsValid()) {
        return kNtStatusInvalidHandle;
    }

    if (bytes_to_read == 0) {
        if (bytes_read_ptr != 0) {
            (void)mem.Write32(bytes_read_ptr, 0);
        }
        return kNtStatusSuccess;
    }

    std::vector<u8> temp_buffer(bytes_to_read);
    auto read_res = vfs_->Read(handle, temp_buffer);
    if (!read_res) {
        return MapVfsErrorToNtStatus(read_res.error().code);
    }

    size_t actual_read = *read_res;
    if (actual_read > 0) {
        auto write_res = mem.WriteBytes(buffer_addr, ByteSpan(temp_buffer.data(), actual_read));
        if (!write_res) {
            return kNtStatusInvalidParameter;
        }
    }

    if (bytes_read_ptr != 0) {
        auto w_res = mem.Write32(bytes_read_ptr, static_cast<u32>(actual_read));
        if (!w_res) {
            return kNtStatusInvalidParameter;
        }
    }

    return kNtStatusSuccess;
}

Result<u32> KernelFileServices::WriteFile(u32 /*handle_val*/, GuestAddr /*buffer_addr*/,
                                          u32 /*bytes_to_write*/, GuestAddr bytes_written_ptr,
                                          GuestAddr overlapped_ptr, memory::AddressSpace& mem) {
    if (overlapped_ptr != 0) {
        return kNtStatusNotImplemented;
    }

    if (bytes_written_ptr != 0) {
        auto val = mem.ValidateRange(bytes_written_ptr, 4, memory::MemoryPermission::Write);
        if (val) {
            (void)mem.Write32(bytes_written_ptr, 0);
        }
    }

    return kNtStatusAccessDenied;
}

Result<u32> KernelFileServices::CloseHandle(u32 handle_val) {
    if (!vfs_) {
        return kNtStatusUnsuccessful;
    }

    VfsHandle handle = VfsHandle::FromU32(handle_val);
    if (!handle.IsValid()) {
        return kNtStatusInvalidHandle;
    }

    auto close_res = vfs_->Close(handle);
    if (!close_res) {
        return MapVfsErrorToNtStatus(close_res.error().code);
    }

    return kNtStatusSuccess;
}

Result<u32> KernelFileServices::SetFilePointer(u32 handle_val, i32 distance,
                                               GuestAddr high_distance_ptr, u32 move_method,
                                               memory::AddressSpace& mem) {
    if (!vfs_) {
        return kInvalidHandleValue;
    }

    VfsHandle handle = VfsHandle::FromU32(handle_val);
    if (!handle.IsValid()) {
        return kInvalidHandleValue;
    }

    VfsSeekOrigin origin = VfsSeekOrigin::Begin;
    switch (move_method) {
    case 0: // FILE_BEGIN
        origin = VfsSeekOrigin::Begin;
        break;
    case 1: // FILE_CURRENT
        origin = VfsSeekOrigin::Current;
        break;
    case 2: // FILE_END
        origin = VfsSeekOrigin::End;
        break;
    default:
        return kInvalidHandleValue;
    }

    i64 total_distance = distance;
    if (high_distance_ptr != 0) {
        auto r = mem.Read32(high_distance_ptr);
        if (!r) {
            return kInvalidHandleValue;
        }
        total_distance |= (static_cast<i64>(*r) << 32);
    }

    auto seek_res = vfs_->Seek(handle, total_distance, origin);
    if (!seek_res) {
        return kInvalidHandleValue;
    }

    u64 new_pos = *seek_res;
    if (high_distance_ptr != 0) {
        auto w = mem.Write32(high_distance_ptr, static_cast<u32>(new_pos >> 32));
        if (!w) {
            return kInvalidHandleValue;
        }
    }

    return static_cast<u32>(new_pos & 0xFFFFFFFFU);
}

Result<u32> KernelFileServices::QueryInformationFile(u32 handle_val, GuestAddr info_ptr, u32 length,
                                                     u32 /*info_class*/,
                                                     memory::AddressSpace& mem) {
    if (!vfs_) {
        return kNtStatusUnsuccessful;
    }

    if (length < 24) {
        return kNtStatusInvalidParameter;
    }

    auto val_res = mem.ValidateRange(info_ptr, length, memory::MemoryPermission::Write);
    if (!val_res) {
        return kNtStatusInvalidParameter;
    }

    VfsHandle handle = VfsHandle::FromU32(handle_val);
    if (!handle.IsValid()) {
        return kNtStatusInvalidHandle;
    }

    auto q_res = vfs_->QueryByHandle(handle);
    if (!q_res) {
        return MapVfsErrorToNtStatus(q_res.error().code);
    }

    u64 size = q_res->size;
    u64 alloc_size = (size + 2047ULL) & ~2047ULL;

    (void)mem.Write32(info_ptr + 0, static_cast<u32>(alloc_size & 0xFFFFFFFFU));
    (void)mem.Write32(info_ptr + 4, static_cast<u32>(alloc_size >> 32));
    (void)mem.Write32(info_ptr + 8, static_cast<u32>(size & 0xFFFFFFFFU));
    (void)mem.Write32(info_ptr + 12, static_cast<u32>(size >> 32));
    (void)mem.Write32(info_ptr + 16, 1); // Number of links
    (void)mem.Write8(info_ptr + 20, 0);  // Delete pending
    (void)mem.Write8(info_ptr + 21, q_res->is_directory ? 1 : 0);

    return kNtStatusSuccess;
}

Result<u32> KernelFileServices::QueryDirectoryFile(u32 handle_val, GuestAddr out_entry_ptr,
                                                   u32 length, memory::AddressSpace& mem) {
    if (!vfs_) {
        return kNtStatusUnsuccessful;
    }

    if (length < 28) {
        return kNtStatusInvalidParameter;
    }

    auto val_res = mem.ValidateRange(out_entry_ptr, length, memory::MemoryPermission::Write);
    if (!val_res) {
        return kNtStatusInvalidParameter;
    }

    VfsHandle handle = VfsHandle::FromU32(handle_val);
    if (!handle.IsValid()) {
        return kNtStatusInvalidHandle;
    }

    std::vector<VfsDirEntry> entries;
    auto enum_res = vfs_->EnumerateDirectory(handle, entries);
    if (!enum_res) {
        return MapVfsErrorToNtStatus(enum_res.error().code);
    }

    if (entries.empty()) {
        return 0x80000006U; // STATUS_NO_MORE_FILES
    }

    const auto& ent = entries.front();
    u32 name_len = static_cast<u32>(std::min<size_t>(ent.name.size(), length - 24));

    (void)mem.Write32(out_entry_ptr + 0, 0); // NextEntryOffset
    (void)mem.Write32(out_entry_ptr + 4, 0); // FileIndex
    (void)mem.Write32(out_entry_ptr + 8, static_cast<u32>(ent.size & 0xFFFFFFFFU));
    (void)mem.Write32(out_entry_ptr + 12, static_cast<u32>(ent.size >> 32));
    (void)mem.Write32(out_entry_ptr + 16, ent.is_directory ? 0x10U : 0x20U);
    (void)mem.Write32(out_entry_ptr + 20, name_len);

    for (u32 i = 0; i < name_len; ++i) {
        (void)mem.Write8(out_entry_ptr + 24 + i, static_cast<u8>(ent.name[i]));
    }

    return kNtStatusSuccess;
}

Result<u32> KernelFileServices::DeviceIoControl(u32 /*handle_val*/, u32 /*control_code*/,
                                                GuestAddr /*in_buf*/, u32 /*in_len*/,
                                                GuestAddr /*out_buf*/, u32 /*out_len*/,
                                                GuestAddr /*bytes_returned_ptr*/,
                                                GuestAddr overlapped_ptr,
                                                memory::AddressSpace& /*mem*/) {
    if (overlapped_ptr != 0) {
        return kNtStatusNotImplemented;
    }
    return kNtStatusNotImplemented;
}

} // namespace xblob::kernel
