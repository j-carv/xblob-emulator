#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/vfs/vfs.hpp"

#include <memory>

namespace xblob::kernel {

// Standard NT status codes (clean-room public values)
constexpr u32 kNtStatusSuccess = 0x00000000;
constexpr u32 kNtStatusNotImplemented = 0xC0000002;
constexpr u32 kNtStatusInvalidHandle = 0xC0000008;
constexpr u32 kNtStatusInvalidParameter = 0xC000000D;
constexpr u32 kNtStatusEndOfFile = 0xC0000011;
constexpr u32 kNtStatusAccessDenied = 0xC0000022;
constexpr u32 kNtStatusObjectNameNotFound = 0xC0000034;
constexpr u32 kNtStatusNotADirectory = 0xC0000103;
constexpr u32 kNtStatusFileIsADirectory = 0xC00000BA;
constexpr u32 kNtStatusUnsuccessful = 0xC0000001;

// Win32 constants
constexpr u32 kInvalidHandleValue = 0xFFFFFFFF;

[[nodiscard]] u32 MapVfsErrorToNtStatus(ErrorCode code) noexcept;

class KernelFileServices {
public:
    explicit KernelFileServices(std::shared_ptr<Vfs> vfs) noexcept : vfs_(std::move(vfs)) {}

    void SetVfs(std::shared_ptr<Vfs> vfs) noexcept { vfs_ = std::move(vfs); }
    [[nodiscard]] const std::shared_ptr<Vfs>& vfs() const noexcept { return vfs_; }

    // Synchronous transactional file operations
    Result<u32> CreateFile(GuestAddr path_ptr, u32 desired_access, u32 disposition,
                           GuestAddr out_handle_ptr, memory::AddressSpace& mem);

    Result<u32> ReadFile(u32 handle_val, GuestAddr buffer_addr, u32 bytes_to_read,
                         GuestAddr bytes_read_ptr, GuestAddr overlapped_ptr,
                         memory::AddressSpace& mem);

    Result<u32> WriteFile(u32 handle_val, GuestAddr buffer_addr, u32 bytes_to_write,
                          GuestAddr bytes_written_ptr, GuestAddr overlapped_ptr,
                          memory::AddressSpace& mem);

    Result<u32> CloseHandle(u32 handle_val);

    Result<u32> SetFilePointer(u32 handle_val, i32 distance, GuestAddr high_distance_ptr,
                               u32 move_method, memory::AddressSpace& mem);

    Result<u32> QueryInformationFile(u32 handle_val, GuestAddr info_ptr, u32 length, u32 info_class,
                                     memory::AddressSpace& mem);

    Result<u32> QueryDirectoryFile(u32 handle_val, GuestAddr out_entry_ptr, u32 length,
                                   memory::AddressSpace& mem);

    Result<u32> DeviceIoControl(u32 handle_val, u32 control_code, GuestAddr in_buf, u32 in_len,
                                GuestAddr out_buf, u32 out_len, GuestAddr bytes_returned_ptr,
                                GuestAddr overlapped_ptr, memory::AddressSpace& mem);

private:
    std::shared_ptr<Vfs> vfs_;
};

} // namespace xblob::kernel
