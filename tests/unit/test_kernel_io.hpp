#pragma once

#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/kernel/kernel_hle.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/vfs/xdvdfs_vfs_volume.hpp"

namespace xblob::kernel {

inline std::shared_ptr<Vfs> CreateTestKernelVfs() {
    auto vfs = std::make_shared<Vfs>();
    auto xbe_data = testing::CreateValidSyntheticXbe(0x55667788, "Kernel Test", 1);
    std::vector<u8> cfg_data = {'A', 'B', 'C', 'D'};
    auto img = testing::BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", xbe_data},
        {"SYSTEM/CONFIG.INI", cfg_data},
    });
    auto source = std::make_shared<VectorByteSource>(std::move(img));
    auto vol_res = XdvdfsVolume::Open(source);
    if (!vol_res) {
        return nullptr;
    }
    auto vfs_vol = std::make_shared<XdvdfsVfsVolume>(*vol_res);
    (void)vfs->Mount("D", vfs_vol);
    return vfs;
}

inline void TestKernelFileServicesBasicLifecycleImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    EXPECT_TRUE(vfs != nullptr);
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;

    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\default.xbe";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto create_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(create_res.has_value());
    EXPECT_EQ(*create_res, kNtStatusSuccess);

    auto handle_val = mem.Read32(0x2000);
    EXPECT_TRUE(handle_val.has_value());
    u32 handle = *handle_val;
    EXPECT_NE(handle, kInvalidHandleValue);

    auto read_res = kernel.file_services().ReadFile(handle, 0x3000, 4, 0x2004, 0, mem);
    EXPECT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, kNtStatusSuccess);

    auto bytes_read = mem.Read32(0x2004);
    EXPECT_TRUE(bytes_read.has_value());
    EXPECT_EQ(*bytes_read, 4U);

    auto magic = mem.Read32(0x3000);
    EXPECT_TRUE(magic.has_value());
    EXPECT_EQ(*magic, 0x48454258U);

    auto close_res = kernel.file_services().CloseHandle(handle);
    EXPECT_TRUE(close_res.has_value());
    EXPECT_EQ(*close_res, kNtStatusSuccess);

    auto close_dup = kernel.file_services().CloseHandle(handle);
    EXPECT_TRUE(close_dup.has_value());
    EXPECT_EQ(*close_dup, kNtStatusInvalidHandle);
}

inline void TestKernelFileServicesPointerAndBoundsImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 handle = *mem.Read32(0x2000);

    auto seek_res = kernel.file_services().SetFilePointer(handle, 2, 0, 0, mem);
    EXPECT_TRUE(seek_res.has_value());
    EXPECT_EQ(*seek_res, 2U);

    auto read_res = kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, kNtStatusSuccess);
    EXPECT_EQ(*mem.Read8(0x3000), 'C');
    EXPECT_EQ(*mem.Read8(0x3001), 'D');

    auto read_eof = kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(read_eof.has_value());
    EXPECT_EQ(*read_eof, kNtStatusEndOfFile);

    (void)kernel.file_services().CloseHandle(handle);
}

inline void TestKernelFileServicesTransactionalMemoryImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 handle = *mem.Read32(0x2000);

    auto bad_read = kernel.file_services().ReadFile(handle, 0xF0000000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(bad_read.has_value());
    EXPECT_EQ(*bad_read, kNtStatusInvalidParameter);

    auto read_ok = kernel.file_services().ReadFile(handle, 0x3000, 1, 0x2004, 0, mem);
    EXPECT_TRUE(read_ok.has_value());
    EXPECT_EQ(*read_ok, kNtStatusSuccess);
    EXPECT_EQ(*mem.Read8(0x3000), 'A');

    (void)kernel.file_services().CloseHandle(handle);
}

inline void TestKernelFileServicesAsyncAndWriteRejectionImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_write = kernel.file_services().CreateFile(0x1000, 0x40000000U, 3, 0x2000, mem);
    EXPECT_TRUE(open_write.has_value());
    EXPECT_EQ(*open_write, kNtStatusAccessDenied);

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 handle = *mem.Read32(0x2000);

    auto async_read = kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0x5000, mem);
    EXPECT_TRUE(async_read.has_value());
    EXPECT_EQ(*async_read, kNtStatusNotImplemented);

    auto write_res = kernel.file_services().WriteFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(write_res.has_value());
    EXPECT_EQ(*write_res, kNtStatusAccessDenied);

    (void)kernel.file_services().CloseHandle(handle);
}

inline void TestKernelFileServicesQueryAndDirectoryEnumerationImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 file_h = *mem.Read32(0x2000);

    auto q_res = kernel.file_services().QueryInformationFile(file_h, 0x3000, 32, 5, mem);
    EXPECT_TRUE(q_res.has_value());
    EXPECT_EQ(*q_res, kNtStatusSuccess);

    auto end_of_file = mem.Read32(0x3008);
    EXPECT_TRUE(end_of_file.has_value());
    EXPECT_EQ(*end_of_file, 4U);

    (void)kernel.file_services().CloseHandle(file_h);

    auto dir_vh = vfs->OpenDirectory("D:\\system");
    EXPECT_TRUE(dir_vh.has_value());

    auto dir_q = kernel.file_services().QueryDirectoryFile(dir_vh->ToU32(), 0x4000, 64, mem);
    EXPECT_TRUE(dir_q.has_value());
    EXPECT_EQ(*dir_q, kNtStatusSuccess);

    (void)vfs->Close(*dir_vh);
}

inline void TestKernelFileServicesTwoThreadsGuestImpl() {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    memory::Ram& ram = *ram_res;
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto t1 = kernel.threads().CreateThread(0x100, 0x7000, 0);
    EXPECT_TRUE(t1.has_value());
    u32 tid1 = *t1;

    auto t2 = kernel.threads().CreateThread(0x200, 0x8000, 0);
    EXPECT_TRUE(t2.has_value());
    u32 tid2 = *t2;

    cpu::CpuContext active_ctx;
    EXPECT_EQ(kernel.threads().current_thread_id(), tid1);
    auto open1 = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open1.has_value());
    u32 h1 = *mem.Read32(0x2000);

    auto r1 = kernel.file_services().ReadFile(h1, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(*mem.Read32(0x2004), 2U);

    EXPECT_TRUE(kernel.threads().Yield(active_ctx).has_value());
    EXPECT_EQ(kernel.threads().current_thread_id(), tid2);

    auto open2 = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2010, mem);
    EXPECT_TRUE(open2.has_value());
    u32 h2 = *mem.Read32(0x2010);
    EXPECT_NE(h1, h2);

    auto r2 = kernel.file_services().ReadFile(h2, 0x3010, 4, 0x2014, 0, mem);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(*mem.Read32(0x2014), 4U);
    EXPECT_EQ(*mem.Read8(0x3010), 'A');

    EXPECT_EQ(*kernel.file_services().CloseHandle(h2), kNtStatusSuccess);
    EXPECT_EQ(*kernel.file_services().CloseHandle(h1), kNtStatusSuccess);
}

} // namespace xblob::kernel
