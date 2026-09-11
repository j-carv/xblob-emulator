#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/kernel/debug_sink.hpp"
#include "xblob/kernel/guest_heap.hpp"
#include "xblob/kernel/kernel_hle.hpp"
#include "xblob/kernel/registry.hpp"
#include "xblob/kernel/sync_objects.hpp"
#include "xblob/kernel/thread_scheduler.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/vfs/xdvdfs_vfs_volume.hpp"

using namespace xblob;
using namespace xblob::kernel;
using namespace xblob::memory;

TEST_CASE(TestKernelRegistryAndDuplicates) {
    ExportRegistry reg;
    EXPECT_EQ(reg.count(), 0U);

    EXPECT_TRUE(
        reg.RegisterExport(12, "DbgPrint", 1, [](GuestContext&) -> Result<u32> { return 0; })
            .has_value());
    EXPECT_EQ(reg.count(), 1U);
    EXPECT_TRUE(reg.HasOrdinal(12));
    EXPECT_TRUE(!reg.HasOrdinal(999));

    // Duplicate registration MUST fail
    auto dup =
        reg.RegisterExport(12, "Duplicate", 1, [](GuestContext&) -> Result<u32> { return 0; });
    EXPECT_TRUE(!dup.has_value());
    EXPECT_EQ(dup.error().code, ErrorCode::InvalidArgument);

    // Ordinal not found returns nullptr
    EXPECT_TRUE(reg.FindByOrdinal(999) == nullptr);
    const auto* entry = reg.FindByOrdinal(12);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->name, "DbgPrint");
    EXPECT_EQ(entry->param_count, 1U);
}

TEST_CASE(TestDebugSinkBoundedAndSanitizer) {
    BufferedDebugSink sink(3, 30); // Max 3 lines, max 30 bytes

    // 1. Valid string output
    sink.OutputDebugString("Hello ");
    sink.OutputDebugString("World!\n");
    EXPECT_EQ(sink.lines().size(), 2U);
    EXPECT_EQ(sink.GetCombinedOutput(), "Hello World!\n");

    // 2. Sanitization: non-printable bytes converted to '?'
    std::string bad = "Test\x01\x02\xFF.";
    std::string clean = BufferedDebugSink::SanitizeString(bad);
    EXPECT_EQ(clean, "Test???.");

    // 3. Buffer capacity and line dropping
    sink.OutputDebugString("Line 3\n");
    sink.OutputDebugString("Line 4\n"); // Should drop Line 1
    EXPECT_EQ(sink.lines().size(), 3U);
    EXPECT_TRUE(sink.dropped_count() >= 1U);

    sink.Clear();
    EXPECT_EQ(sink.lines().size(), 0U);
    EXPECT_EQ(sink.total_bytes(), 0U);
}

TEST_CASE(TestGuestHeapAllocationAndFree) {
    GuestHeap heap(0x10000000, 0x1000, 8); // 4 KiB heap, 8-byte alignment
    EXPECT_EQ(heap.allocated_bytes(), 0U);
    EXPECT_EQ(heap.free_bytes(), 0x1000U);

    // 1. Allocate 60 bytes -> aligned to 64
    auto a1 = heap.Allocate(60);
    EXPECT_TRUE(a1.has_value());
    EXPECT_EQ(*a1, 0x10000000U);
    EXPECT_EQ(heap.allocated_bytes(), 64U);

    // 2. Allocate 100 bytes -> aligned to 104
    auto a2 = heap.Allocate(100);
    EXPECT_TRUE(a2.has_value());
    EXPECT_EQ(*a2, 0x10000040U);
    EXPECT_EQ(heap.allocated_bytes(), 64U + 104U);

    // 3. Free a1
    EXPECT_TRUE(heap.Free(*a1).has_value());
    EXPECT_EQ(heap.allocated_bytes(), 104U);

    // 4. Double free MUST fail
    auto dfree = heap.Free(*a1);
    EXPECT_TRUE(!dfree.has_value());
    EXPECT_EQ(dfree.error().code, ErrorCode::InvalidArgument);

    // 5. Free unmapped / out-of-bounds address MUST fail
    auto oob = heap.Free(0x20000000);
    EXPECT_TRUE(!oob.has_value());
    EXPECT_EQ(oob.error().code, ErrorCode::OutOfBounds);

    // 6. Free a2 and check complete coalesce
    EXPECT_TRUE(heap.Free(*a2).has_value());
    EXPECT_EQ(heap.allocated_bytes(), 0U);
    EXPECT_EQ(heap.free_bytes(), 0x1000U);

    // 7. Exhaustion check
    auto huge = heap.Allocate(0x2000);
    EXPECT_TRUE(!huge.has_value());
    EXPECT_EQ(huge.error().code, ErrorCode::LimitReached);
}

TEST_CASE(TestSyncObjectsAndGenerationalHandles) {
    HandleTable table;

    // 1. Create Event
    auto ev_h = table.AllocateEvent(false, false);
    EXPECT_TRUE(ev_h.has_value());
    auto ev_obj = table.GetEvent(*ev_h);
    EXPECT_TRUE(ev_obj.has_value());
    EXPECT_TRUE(!(*ev_obj)->is_signaled());

    (*ev_obj)->Set();
    EXPECT_TRUE((*ev_obj)->is_signaled());
    EXPECT_TRUE((*ev_obj)->SatisfyWait(1));
    EXPECT_TRUE(!(*ev_obj)->is_signaled()); // Auto-reset cleared it

    // 2. Create Mutex
    auto m_h = table.AllocateMutex(false, kInvalidThreadId);
    EXPECT_TRUE(m_h.has_value());
    auto m_obj = table.GetMutex(*m_h);
    EXPECT_TRUE(m_obj.has_value());

    // Acquire and recursion
    EXPECT_TRUE((*m_obj)->Acquire(1).value());
    EXPECT_EQ((*m_obj)->recursion_count(), 1U);
    EXPECT_TRUE((*m_obj)->Acquire(1).value()); // Recursive acquire
    EXPECT_EQ((*m_obj)->recursion_count(), 2U);

    // Other thread cannot acquire
    EXPECT_TRUE(!(*m_obj)->Acquire(2).value());

    // Release from thread 1
    EXPECT_TRUE((*m_obj)->Release(1).has_value());
    EXPECT_EQ((*m_obj)->recursion_count(), 1U);
    EXPECT_TRUE((*m_obj)->Release(1).has_value());
    EXPECT_EQ((*m_obj)->recursion_count(), 0U);

    // Release from non-owner fails
    auto unowned = (*m_obj)->Release(2);
    EXPECT_TRUE(!unowned.has_value());

    // 3. Generational handle invalidation
    EXPECT_TRUE(table.CloseHandle(*ev_h).has_value());
    // Closed handle cannot be resolved
    EXPECT_TRUE(!table.GetEvent(*ev_h).has_value());
    // Closing already closed handle fails
    EXPECT_TRUE(!table.CloseHandle(*ev_h).has_value());
}

TEST_CASE(TestThreadSchedulerCooperative) {
    ThreadScheduler sched;

    cpu::CpuContext ctx1;
    ctx1.eip = 0x1000;
    auto t1_res = sched.CreateThread(0x1000, 0x50000, 0);
    EXPECT_TRUE(t1_res.has_value());
    EXPECT_EQ(*t1_res, 1U);

    auto t2_res = sched.CreateThread(0x2000, 0x60000, 0);
    EXPECT_TRUE(t2_res.has_value());
    EXPECT_EQ(*t2_res, 2U);

    EXPECT_EQ(sched.active_thread_count(), 2U);
    EXPECT_EQ(sched.current_thread_id(), 1U);

    // Yield t1: should switch to t2
    cpu::CpuContext active_ctx = ctx1;
    EXPECT_TRUE(sched.Yield(active_ctx).has_value());
    EXPECT_EQ(sched.current_thread_id(), 2U);
    EXPECT_EQ(active_ctx.eip, 0x2000U);

    // Exit t2: should switch back to t1
    EXPECT_TRUE(sched.ExitCurrent(0, active_ctx).has_value());
    EXPECT_EQ(sched.current_thread_id(), 1U);
    EXPECT_EQ(sched.active_thread_count(), 1U);
    EXPECT_EQ(active_ctx.eip, 0x1000U);
}

TEST_CASE(TestKernelHleDispatchThunk) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    auto sink = std::make_shared<BufferedDebugSink>();
    KernelHle kernel(0x1000, 0x2000, sink);

    cpu::CpuContext ctx;
    ctx.SetGpr(cpu::Reg32::ESP, 0x8000);

    // Setup guest string "Clean-Room HLE OK!\n" at 0x500
    const char msg[] = "Clean-Room HLE OK!\n";
    for (std::size_t i = 0; i < sizeof(msg); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x500 + static_cast<GuestAddr>(i), static_cast<u8>(msg[i])).has_value());
    }

    // Stack frame for DbgPrint (ordinal 12, 1 parameter):
    // [ESP+0] return address = 0x200
    // [ESP+4] string pointer = 0x500
    EXPECT_TRUE(mem.Write32(0x8000, 0x200).has_value());
    EXPECT_TRUE(mem.Write32(0x8004, 0x500).has_value());

    auto disp_res = kernel.DispatchThunk(12, ctx, mem);
    EXPECT_TRUE(disp_res.has_value());
    EXPECT_EQ(*disp_res, kStatusSuccess);
    EXPECT_EQ(ctx.GetGpr(cpu::Reg32::EAX), kStatusSuccess);
    EXPECT_EQ(ctx.eip, 0x200U);
    EXPECT_EQ(ctx.GetGpr(cpu::Reg32::ESP), 0x8008U); // Cleaned 4 bytes ret + 4 bytes arg

    EXPECT_EQ(sink->GetCombinedOutput(), "Clean-Room HLE OK!\n");

    // Unknown ordinal dispatch MUST fail gracefully
    auto unk = kernel.DispatchThunk(9999, ctx, mem);
    EXPECT_TRUE(!unk.has_value());
    EXPECT_EQ(unk.error().code, ErrorCode::UnsupportedFeature);
}

namespace {

std::shared_ptr<Vfs> CreateTestKernelVfs() {
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

} // namespace

TEST_CASE(TestKernelFileServicesExportsRegistered) {
    KernelHle kernel;
    EXPECT_TRUE(kernel.registry().HasOrdinal(18));  // NtClose
    EXPECT_TRUE(kernel.registry().HasOrdinal(190)); // NtCreateFile
    EXPECT_TRUE(kernel.registry().HasOrdinal(219)); // NtReadFile
    EXPECT_TRUE(kernel.registry().HasOrdinal(256)); // NtWriteFile
    EXPECT_TRUE(kernel.registry().HasOrdinal(224)); // SetFilePointer
    EXPECT_TRUE(kernel.registry().HasOrdinal(196)); // NtDeviceIoControlFile
}

TEST_CASE(TestKernelFileServicesCreateReadSeekClose) {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    EXPECT_TRUE(vfs != nullptr);
    kernel.SetVfs(vfs);

    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = *ram_res;
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    // Write path string to guest memory at 0x1000
    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    // 1. CreateFile / Open
    // out_handle_ptr = 0x2000, desired_access = 0 (read), disp = 3 (OPEN_EXISTING)
    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    EXPECT_EQ(*open_res, kNtStatusSuccess);

    auto h_res = mem.Read32(0x2000);
    EXPECT_TRUE(h_res.has_value());
    u32 handle = *h_res;
    EXPECT_TRUE(handle != 0);

    // 2. ReadFile
    // Buffer at 0x3000 (length 2), bytes_read_ptr at 0x2004, overlapped = 0
    auto read_res = kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, kNtStatusSuccess);

    auto bytes_read = mem.Read32(0x2004);
    EXPECT_TRUE(bytes_read.has_value());
    EXPECT_EQ(*bytes_read, 2U);

    auto b0 = mem.Read8(0x3000);
    auto b1 = mem.Read8(0x3001);
    EXPECT_TRUE(b0.has_value());
    EXPECT_TRUE(b1.has_value());
    EXPECT_EQ(*b0, 'A');
    EXPECT_EQ(*b1, 'B');

    // 3. SetFilePointer to begin (move_method = 0)
    auto seek_res = kernel.file_services().SetFilePointer(handle, 0, 0, 0, mem);
    EXPECT_TRUE(seek_res.has_value());
    EXPECT_EQ(*seek_res, 0U);

    // 4. CloseHandle
    auto close_res = kernel.file_services().CloseHandle(handle);
    EXPECT_TRUE(close_res.has_value());
    EXPECT_EQ(*close_res, kNtStatusSuccess);

    // Stale handle read fails
    auto stale_res = kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(stale_res.has_value());
    EXPECT_EQ(*stale_res, kNtStatusInvalidHandle);
}

TEST_CASE(TestKernelFileServicesBufferValidationRollback) {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = *ram_res;
    AddressSpace mem;
    // Map only 0x0000..0x8000
    EXPECT_TRUE(mem.MapRam(0, 32 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 handle = *mem.Read32(0x2000);

    // Destination buffer is unmapped (0x90000)
    auto read_bad = kernel.file_services().ReadFile(handle, 0x90000, 4, 0x2004, 0, mem);
    EXPECT_TRUE(read_bad.has_value());
    EXPECT_EQ(*read_bad, kNtStatusInvalidParameter);

    // VFS position must NOT have advanced! A subsequent valid read will still read from offset 0
    auto read_ok = kernel.file_services().ReadFile(handle, 0x3000, 1, 0x2004, 0, mem);
    EXPECT_TRUE(read_ok.has_value());
    EXPECT_EQ(*read_ok, kNtStatusSuccess);
    EXPECT_EQ(*mem.Read8(0x3000), 'A');

    (void)kernel.file_services().CloseHandle(handle);
}

TEST_CASE(TestKernelFileServicesAsyncAndWriteRejection) {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = *ram_res;
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    // 1. Write access request in CreateFile must return AccessDenied
    auto open_write = kernel.file_services().CreateFile(0x1000, 0x40000000U, 3, 0x2000, mem);
    EXPECT_TRUE(open_write.has_value());
    EXPECT_EQ(*open_write, kNtStatusAccessDenied);

    // Open read-only
    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 handle = *mem.Read32(0x2000);

    // 2. Async read rejection
    auto async_read =
        kernel.file_services().ReadFile(handle, 0x3000, 2, 0x2004, 0x5000 /* overlapped */, mem);
    EXPECT_TRUE(async_read.has_value());
    EXPECT_EQ(*async_read, kNtStatusNotImplemented);

    // 3. WriteFile rejection
    auto write_res = kernel.file_services().WriteFile(handle, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(write_res.has_value());
    EXPECT_EQ(*write_res, kNtStatusAccessDenied);

    (void)kernel.file_services().CloseHandle(handle);
}

TEST_CASE(TestKernelFileServicesQueryAndDirectoryEnumeration) {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = *ram_res;
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    auto open_res = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open_res.has_value());
    u32 file_h = *mem.Read32(0x2000);

    // Query file information into guest memory at 0x3000
    auto q_res = kernel.file_services().QueryInformationFile(file_h, 0x3000, 32, 5, mem);
    EXPECT_TRUE(q_res.has_value());
    EXPECT_EQ(*q_res, kNtStatusSuccess);

    auto end_of_file = mem.Read32(0x3008);
    EXPECT_TRUE(end_of_file.has_value());
    EXPECT_EQ(*end_of_file, 4U); // "ABCD" length

    auto is_dir = mem.Read8(0x3015);
    EXPECT_TRUE(is_dir.has_value());
    EXPECT_EQ(*is_dir, 0U);

    (void)kernel.file_services().CloseHandle(file_h);

    // Open directory D:\system
    auto dir_vh = vfs->OpenDirectory("D:\\system");
    EXPECT_TRUE(dir_vh.has_value());

    auto dir_q = kernel.file_services().QueryDirectoryFile(dir_vh->ToU32(), 0x4000, 64, mem);
    EXPECT_TRUE(dir_q.has_value());
    EXPECT_EQ(*dir_q, kNtStatusSuccess);

    auto entry_size = mem.Read32(0x4008);
    EXPECT_TRUE(entry_size.has_value());
    EXPECT_EQ(*entry_size, 4U);

    (void)vfs->Close(*dir_vh);
}

TEST_CASE(TestKernelFileServicesTwoThreadsGuest) {
    KernelHle kernel;
    auto vfs = CreateTestKernelVfs();
    kernel.SetVfs(vfs);

    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = *ram_res;
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, ram, 0).has_value());

    const char path[] = "D:\\system\\config.ini";
    for (size_t i = 0; i < sizeof(path); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x1000 + static_cast<GuestAddr>(i), static_cast<u8>(path[i])).has_value());
    }

    // Thread 1
    auto t1 = kernel.threads().CreateThread(0x100, 0x7000, 0);
    EXPECT_TRUE(t1.has_value());
    u32 tid1 = *t1;

    // Thread 2
    auto t2 = kernel.threads().CreateThread(0x200, 0x8000, 0);
    EXPECT_TRUE(t2.has_value());
    u32 tid2 = *t2;

    cpu::CpuContext active_ctx;
    EXPECT_EQ(kernel.threads().current_thread_id(), tid1);
    auto open1 = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2000, mem);
    EXPECT_TRUE(open1.has_value());
    u32 h1 = *mem.Read32(0x2000);

    // Read 2 bytes in Thread 1
    auto r1 = kernel.file_services().ReadFile(h1, 0x3000, 2, 0x2004, 0, mem);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(*mem.Read32(0x2004), 2U);

    // Switch to Thread 2
    EXPECT_TRUE(kernel.threads().Yield(active_ctx).has_value());
    EXPECT_EQ(kernel.threads().current_thread_id(), tid2);

    // Thread 2 opens same file (independent handle!)
    auto open2 = kernel.file_services().CreateFile(0x1000, 0, 3, 0x2010, mem);
    EXPECT_TRUE(open2.has_value());
    u32 h2 = *mem.Read32(0x2010);
    EXPECT_NE(h1, h2);

    // Thread 2 reads from offset 0
    auto r2 = kernel.file_services().ReadFile(h2, 0x3010, 4, 0x2014, 0, mem);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(*mem.Read32(0x2014), 4U);
    EXPECT_EQ(*mem.Read8(0x3010), 'A');

    // Close both handles
    EXPECT_EQ(*kernel.file_services().CloseHandle(h2), kNtStatusSuccess);
    EXPECT_EQ(*kernel.file_services().CloseHandle(h1), kNtStatusSuccess);
}

int main() {
    return xblob::testing::RunAllTests();
}
