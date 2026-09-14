#pragma once

#include "tests/test_framework.hpp"
#include "xblob/kernel/kernel_hle.hpp"
#include "xblob/kernel/virtual_memory_hle.hpp"
#include "xblob/memory/ram.hpp"

namespace xblob::kernel {

inline void TestVirtualMemoryAllocateFreeQueryProtectImpl() {
    VirtualMemoryManager vm;
    memory::AddressSpace mem;

    // 1. Valid allocation: reserve + commit 8 KiB at 0x00020000
    auto alloc1 = vm.Allocate(0x00020000, 8192, kMemReserve | kMemCommit, kPageReadWrite, mem);
    EXPECT_TRUE(alloc1.has_value());
    EXPECT_EQ(*alloc1, 0x00020000U);
    EXPECT_TRUE(vm.IsRangeCommitted(0x00020000, 8192));

    // 2. Query region
    auto q1 = vm.Query(0x00020000);
    EXPECT_TRUE(q1.has_value());
    EXPECT_EQ(q1->base_address, 0x00020000U);
    EXPECT_EQ(q1->region_size, 8192U);
    EXPECT_EQ(q1->state, kMemCommit);
    EXPECT_EQ(q1->protect, kPageReadWrite);

    // 3. Protect change
    auto prot_res = vm.Protect(0x00020000, 4096, kPageReadOnly);
    EXPECT_TRUE(prot_res.has_value());
    EXPECT_EQ(*prot_res, kPageReadWrite);
    auto q2 = vm.Query(0x00020000);
    EXPECT_TRUE(q2.has_value());
    EXPECT_EQ(q2->protect, kPageReadOnly);

    // 4. Commit inválido: range overlaps existing allocated region
    auto col = vm.Allocate(0x00020000, 4096, kMemReserve, kPageReadWrite, mem);
    EXPECT_TRUE(!col.has_value());
    EXPECT_EQ(col.error().code, ErrorCode::InvalidArgument);

    // 5. Overflow commit fails atomically
    auto ovf = vm.Allocate(0xFFFFF000, 0x2000, kMemReserve, kPageReadWrite, mem);
    EXPECT_TRUE(!ovf.has_value());
    EXPECT_EQ(ovf.error().code, ErrorCode::OutOfBounds);

    // 6. Free with MEM_RELEASE
    auto free_res = vm.Free(0x00020000, 0, kMemRelease, mem);
    EXPECT_TRUE(free_res.has_value());
    EXPECT_EQ(vm.region_count(), 0U);

    // 7. Double free fails
    auto dfree = vm.Free(0x00020000, 0, kMemRelease, mem);
    EXPECT_TRUE(!dfree.has_value());
    EXPECT_EQ(dfree.error().code, ErrorCode::InvalidArgument);
}

inline void TestGuestHeapExpandedImpl() {
    GuestHeapManager mgr(0x10000000, 0x10000);
    auto& heap = mgr.default_heap();

    // 1. Alignment validation: non power-of-2 fails
    auto bad_align = heap.AllocateAligned(100, 7);
    EXPECT_TRUE(!bad_align.has_value());
    EXPECT_EQ(bad_align.error().code, ErrorCode::InvalidArgument);

    // 2. Aligned allocation at 64 bytes
    auto a1 = heap.AllocateAligned(50, 64);
    EXPECT_TRUE(a1.has_value());
    EXPECT_EQ(*a1 % 64, 0U);

    // 3. ReAllocate expansion
    auto realloc_res = heap.ReAllocate(*a1, 200);
    EXPECT_TRUE(realloc_res.has_value());
    GuestAddr a2 = *realloc_res;
    EXPECT_TRUE(heap.IsAllocated(a2));
    EXPECT_TRUE(!heap.IsAllocated(*a1) || a2 == *a1);

    auto size_res = heap.GetBlockSize(a2);
    EXPECT_TRUE(size_res.has_value());
    EXPECT_TRUE(*size_res >= 200U);

    // 4. Secondary heap management
    auto h2 = mgr.CreateHeap(0, 0x8000, 16);
    EXPECT_TRUE(h2.has_value());
    auto sec_heap = mgr.GetHeap(*h2);
    EXPECT_TRUE(sec_heap.has_value());
    auto sec_alloc = (*sec_heap)->Allocate(32);
    EXPECT_TRUE(sec_alloc.has_value());

    EXPECT_TRUE(mgr.DestroyHeap(*h2).has_value());
    EXPECT_TRUE(!mgr.GetHeap(*h2).has_value());
}

inline void TestDeterministicTimeAndTimersImpl() {
    KernelTime ktime;
    ktime.AdvanceCycles(733'333ULL * 10); // 10 ms
    EXPECT_EQ(ktime.QueryTickCount(), 10U);

    TimerObject one_shot(TimerType::SynchronizationTimer);
    one_shot.Set(1000, 0);
    EXPECT_TRUE(!one_shot.is_signaled());
    EXPECT_TRUE(one_shot.is_active());

    // Advance to 500: not signaled
    EXPECT_EQ(one_shot.AdvanceToCycle(500), 0U);
    EXPECT_TRUE(!one_shot.is_signaled());

    // Advance to 1000: signals
    EXPECT_EQ(one_shot.AdvanceToCycle(1000), 1U);
    EXPECT_TRUE(one_shot.is_signaled());
    EXPECT_TRUE(!one_shot.is_active());

    // Satisfy wait: resets signal
    EXPECT_TRUE(one_shot.SatisfyWait(1));
    EXPECT_TRUE(!one_shot.is_signaled());

    // Periodic timer test
    TimerObject periodic(TimerType::NotificationTimer);
    periodic.Set(1000, 10); // 10ms period = 7'333'330 cycles
    Cycle period_cyc = KernelTime::MillisToCycle(10);

    // Advance past 3 periods
    Cycle target = 1000 + (period_cyc * 3) + 50;
    u64 expirations = periodic.AdvanceToCycle(target);
    EXPECT_TRUE(expirations >= 3U);
    EXPECT_TRUE(periodic.is_signaled());
    EXPECT_TRUE(periodic.is_active());

    // Cancellation leaves zero orphan events
    EXPECT_TRUE(periodic.Cancel());
    EXPECT_TRUE(!periodic.is_active());
    EXPECT_TRUE(!periodic.is_signaled());
}

inline void TestSyncSemaphoreAndFifoWakesImpl() {
    HandleTable handles;
    auto sem_res = handles.AllocateSemaphore(1, 2);
    EXPECT_TRUE(sem_res.has_value());
    GuestHandle sem_h = *sem_res;

    // Acquire initial token
    EXPECT_TRUE(handles.SatisfyWait(sem_h, 1));
    EXPECT_TRUE(!handles.IsObjectSignaled(sem_h, 1));

    // Register waiting threads FIFO: thread 2 then thread 3
    handles.AddWaitingThread(sem_h, 2);
    handles.AddWaitingThread(sem_h, 3);

    auto* sem = *handles.GetSemaphore(sem_h);
    EXPECT_EQ(sem->waiting_threads().size(), 2U);
    EXPECT_EQ(sem->waiting_threads().front(), 2U);

    // Release 1: wakes thread 2 first in FIFO order
    i32 prev = 0;
    EXPECT_TRUE(sem->Release(1, &prev).has_value());
    EXPECT_EQ(prev, 0);

    // Release exceeding max_count MUST fail atomically
    auto over_rel = sem->Release(5, &prev);
    EXPECT_TRUE(!over_rel.has_value());
    EXPECT_EQ(over_rel.error().code, ErrorCode::LimitReached);
    EXPECT_EQ(sem->current_count(), 1); // Unchanged count
}

inline void TestMutantOwnershipAndRecursionImpl() {
    HandleTable handles;
    auto m_res = handles.AllocateMutex(false, kInvalidThreadId);
    EXPECT_TRUE(m_res.has_value());
    GuestHandle mut_h = *m_res;
    auto* mut = *handles.GetMutex(mut_h);

    // Thread 1 acquires
    EXPECT_TRUE(mut->Acquire(1).value());
    EXPECT_EQ(mut->recursion_count(), 1U);
    EXPECT_EQ(mut->owner(), 1U);

    // Thread 1 recursive acquire
    EXPECT_TRUE(mut->Acquire(1).value());
    EXPECT_EQ(mut->recursion_count(), 2U);

    // Thread 2 cannot acquire
    EXPECT_TRUE(!mut->Acquire(2).value());

    // Thread 2 cannot release
    EXPECT_TRUE(!mut->Release(2).has_value());

    // Thread 1 releases once
    EXPECT_TRUE(mut->Release(1).has_value());
    EXPECT_EQ(mut->recursion_count(), 1U);
    EXPECT_TRUE(mut->is_locked());

    // Thread 1 releases second time -> unlocked
    EXPECT_TRUE(mut->Release(1).has_value());
    EXPECT_EQ(mut->recursion_count(), 0U);
    EXPECT_TRUE(!mut->is_locked());
}

inline void TestStaleHandleInvalidationImpl() {
    HandleTable handles;
    auto ev_res = handles.AllocateEvent(false, false);
    EXPECT_TRUE(ev_res.has_value());
    GuestHandle h1 = *ev_res;
    EXPECT_TRUE(handles.IsValidHandle(h1));

    // Close handle
    EXPECT_TRUE(handles.CloseHandle(h1).has_value());

    // Handle is now stale
    EXPECT_TRUE(!handles.IsValidHandle(h1));
    EXPECT_TRUE(!handles.GetEvent(h1).has_value());

    // Reallocate will use same slot with new generation
    auto ev2 = handles.AllocateEvent(false, false);
    EXPECT_TRUE(ev2.has_value());
    GuestHandle h2 = *ev2;
    EXPECT_NE(h1, h2);
    EXPECT_TRUE(handles.IsValidHandle(h2));
    EXPECT_TRUE(!handles.IsValidHandle(h1)); // Old handle still invalid!
}

inline void TestUnsupportedExportAndRegistryStatsImpl() {
    KernelHle kernel;
    cpu::CpuContext cpu;
    cpu.Reset();
    cpu.eip = 0x00401000;
    cpu.SetGpr(cpu::Reg32::ESP, 0x8000);

    auto ram = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram.has_value());
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, *ram, 0).has_value());

    // Write return address and args onto stack
    (void)mem.Write32(0x8000, 0x00402000);
    (void)mem.Write32(0x8004, 0x1111);
    (void)mem.Write32(0x8008, 0x2222);

    auto disp_res = kernel.DispatchThunk(0x9999, cpu, mem);
    EXPECT_TRUE(!disp_res.has_value());
    EXPECT_EQ(disp_res.error().code, ErrorCode::UnsupportedFeature);

    const auto& last = kernel.registry().last_unsupported_export();
    EXPECT_TRUE(last.has_value());
    EXPECT_EQ(last->ordinal, 0x9999U);
    EXPECT_EQ(last->eip, 0x00401000U);
    EXPECT_TRUE(last->recorded_args.size() >= 2U);
    EXPECT_EQ(last->recorded_args[0], 0x1111U);
    EXPECT_EQ(last->recorded_args[1], 0x2222U);

    auto stats = kernel.registry().GetStats();
    EXPECT_EQ(stats.unsupported_dispatches, 1U);
    EXPECT_EQ(stats.failed_dispatches, 0U);
}

inline void TestTwoRunsDeterminismImpl() {
    auto run_simulation = []() -> std::tuple<Cycle, u64, u32> {
        KernelHle kernel;
        cpu::CpuContext cpu;
        cpu.Reset();

        // Advance 5 steps of 100,000 cycles with timer
        auto timer_h = kernel.handles().AllocateTimer(TimerType::SynchronizationTimer);
        auto* timer = *kernel.handles().GetTimer(*timer_h);
        timer->Set(150'000, 50'000);

        for (int i = 0; i < 5; ++i) {
            kernel.time().AdvanceCycles(100'000);
            (void)timer->AdvanceToCycle(kernel.time().current_cycle());
        }

        return {kernel.time().current_cycle(), kernel.time().QuerySystemTime(),
                kernel.time().QueryTickCount()};
    };

    auto run1 = run_simulation();
    auto run2 = run_simulation();

    EXPECT_EQ(std::get<0>(run1), std::get<0>(run2));
    EXPECT_EQ(std::get<1>(run1), std::get<1>(run2));
    EXPECT_EQ(std::get<2>(run1), std::get<2>(run2));
}

inline void TestDeviceIoControlAndSyncIOImpl() {
    KernelHle kernel;
    cpu::CpuContext cpu;
    cpu.Reset();
    cpu.SetGpr(cpu::Reg32::ESP, 0x8000);

    auto ram = memory::Ram::Create(memory::kRamSizeRetail);
    EXPECT_TRUE(ram.has_value());
    memory::AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0, 64 * 1024, *ram, 0).has_value());

    // 1. Stack for NtDeviceIoControlFile:
    // [ESP+0] return address
    // [ESP+4] handle: 0 (invalid)
    // [ESP+8] code: 0x00070000 (IOCTL_DISK_GET_DRIVE_GEOMETRY)
    // [ESP+12] in_buf: 0
    // [ESP+16] in_len: 0
    // [ESP+20] out_buf: 0x3000
    // [ESP+24] out_len: 24
    // [ESP+28] ret_ptr: 0x3020
    // [ESP+32] ovl: 0x5000 (non-zero overlapped -> unsupported async honest)
    (void)mem.Write32(0x8000, 0x00401000);
    (void)mem.Write32(0x8004, 1);
    (void)mem.Write32(0x8008, 0x00070000);
    (void)mem.Write32(0x800C, 0);
    (void)mem.Write32(0x8010, 0);
    (void)mem.Write32(0x8014, 0x3000);
    (void)mem.Write32(0x8018, 24);
    (void)mem.Write32(0x801C, 0x3020);
    (void)mem.Write32(0x8020, 0x5000); // Async overlapped!

    auto res = kernel.DispatchThunk(196, cpu, mem);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(*res, kNtStatusNotImplemented); // Honest async rejection
}

} // namespace xblob::kernel
