#include "tests/test_framework.hpp"
#include "tests/unit/test_kernel_expanded.hpp"
#include "tests/unit/test_kernel_io.hpp"
#include "xblob/kernel/debug_sink.hpp"
#include "xblob/kernel/guest_heap.hpp"
#include "xblob/kernel/kernel_hle.hpp"
#include "xblob/kernel/registry.hpp"
#include "xblob/kernel/sync_objects.hpp"
#include "xblob/kernel/thread_scheduler.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"

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
    EXPECT_TRUE(!table.GetEvent(*ev_h).has_value());
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

    const char msg[] = "Clean-Room HLE OK!\n";
    for (std::size_t i = 0; i < sizeof(msg); ++i) {
        EXPECT_TRUE(
            mem.Write8(0x500 + static_cast<GuestAddr>(i), static_cast<u8>(msg[i])).has_value());
    }

    // Stack frame for DbgPrint (ordinal 12, 1 parameter):
    EXPECT_TRUE(mem.Write32(0x8000, 0x200).has_value());
    EXPECT_TRUE(mem.Write32(0x8004, 0x500).has_value());

    auto disp_res = kernel.DispatchThunk(12, ctx, mem);
    EXPECT_TRUE(disp_res.has_value());
    EXPECT_EQ(*disp_res, kStatusSuccess);
    EXPECT_EQ(ctx.GetGpr(cpu::Reg32::EAX), kStatusSuccess);
    EXPECT_EQ(ctx.eip, 0x200U);
    EXPECT_EQ(ctx.GetGpr(cpu::Reg32::ESP), 0x8008U);

    EXPECT_EQ(sink->GetCombinedOutput(), "Clean-Room HLE OK!\n");

    // Unknown ordinal dispatch MUST fail gracefully
    auto unk = kernel.DispatchThunk(9999, ctx, mem);
    EXPECT_TRUE(!unk.has_value());
    EXPECT_EQ(unk.error().code, ErrorCode::UnsupportedFeature);
}

TEST_CASE(TestKernelFileServicesBasicLifecycle) {
    TestKernelFileServicesBasicLifecycleImpl();
}

TEST_CASE(TestKernelFileServicesPointerAndBounds) {
    TestKernelFileServicesPointerAndBoundsImpl();
}

TEST_CASE(TestKernelFileServicesTransactionalMemory) {
    TestKernelFileServicesTransactionalMemoryImpl();
}

TEST_CASE(TestKernelFileServicesAsyncAndWriteRejection) {
    TestKernelFileServicesAsyncAndWriteRejectionImpl();
}

TEST_CASE(TestKernelFileServicesQueryAndDirectoryEnumeration) {
    TestKernelFileServicesQueryAndDirectoryEnumerationImpl();
}

TEST_CASE(TestKernelFileServicesTwoThreadsGuest) {
    TestKernelFileServicesTwoThreadsGuestImpl();
}

TEST_CASE(TestVirtualMemoryAllocateFreeQueryProtect) {
    TestVirtualMemoryAllocateFreeQueryProtectImpl();
}

TEST_CASE(TestGuestHeapExpanded) {
    TestGuestHeapExpandedImpl();
}

TEST_CASE(TestDeterministicTimeAndTimers) {
    TestDeterministicTimeAndTimersImpl();
}

TEST_CASE(TestSyncSemaphoreAndFifoWakes) {
    TestSyncSemaphoreAndFifoWakesImpl();
}

TEST_CASE(TestMutantOwnershipAndRecursion) {
    TestMutantOwnershipAndRecursionImpl();
}

TEST_CASE(TestStaleHandleInvalidation) {
    TestStaleHandleInvalidationImpl();
}

TEST_CASE(TestUnsupportedExportAndRegistryStats) {
    TestUnsupportedExportAndRegistryStatsImpl();
}

TEST_CASE(TestTwoRunsDeterminism) {
    TestTwoRunsDeterminismImpl();
}

TEST_CASE(TestDeviceIoControlAndSyncIO) {
    TestDeviceIoControlAndSyncIOImpl();
}

int main() {
    return xblob::testing::RunAllTests();
}
