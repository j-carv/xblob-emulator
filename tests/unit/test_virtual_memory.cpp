#include "tests/test_framework.hpp"
#include "xblob/cpu/cpu.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"
#include "xblob/memory/virtual_memory.hpp"

using namespace xblob;
using namespace xblob::memory;
using namespace xblob::cpu;

TEST_CASE(TestVirtualMemoryPagingDisabledIdentity) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x0000, 64 * 1024, ram, 0).has_value());

    VirtualMemory vmem(space);
    EXPECT_FALSE(vmem.is_paging_enabled());

    // Without paging, linear address translates directly to physical address
    auto trans = vmem.Translate(0x1234, VirtualAccessType::Read);
    EXPECT_TRUE(trans.has_value());
    EXPECT_EQ(trans->physical_address, 0x1234u);

    // Reads and writes match physical address
    EXPECT_TRUE(vmem.Write32(0x1000, 0xAABBCCDD).has_value());
    auto r32 = vmem.Read32(0x1000);
    EXPECT_TRUE(r32.has_value());
    EXPECT_EQ(*r32, 0xAABBCCDDu);
}

TEST_CASE(TestPageFaultCodeEncoding) {
    AddressSpace space;
    VirtualMemory vmem(space);
    vmem.SetCr0(kCr0Paging); // Paging enabled
    vmem.SetCr3(0x1000);     // Directory at 0x1000 (which is unmapped in physical space)

    // PDE unmapped physical address returns error
    auto res_unmapped = vmem.Read32(0x20000000);
    EXPECT_FALSE(res_unmapped.has_value());

    // Now map RAM with an empty directory (all entries 0 -> Not Present)
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    EXPECT_TRUE(space.MapRam(0x0000, 64 * 1024, ram, 0).has_value());

    // Supervisor Read with Not Present page:
    // P=0, W=0, U=0, I=0 -> error code 0
    vmem.SetCpl(0);
    auto res_np_read = vmem.Read32(0x00400000);
    EXPECT_FALSE(res_np_read.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_EQ(vmem.last_page_fault()->fault_address, 0x00400000u);
    EXPECT_EQ(vmem.last_page_fault()->error_code, 0u);
    EXPECT_TRUE(vmem.last_page_fault()->is_not_present());
    EXPECT_FALSE(vmem.last_page_fault()->is_write());
    EXPECT_FALSE(vmem.last_page_fault()->is_user());

    // User Write with Not Present page:
    // P=0, W=1 (bit 1), U=1 (bit 2) -> error code 0b110 = 6
    vmem.SetCpl(3);
    auto res_np_write = vmem.Write32(0x00400000, 42);
    EXPECT_FALSE(res_np_write.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_EQ(vmem.last_page_fault()->fault_address, 0x00400000u);
    EXPECT_EQ(vmem.last_page_fault()->error_code, (kPageFaultWrite | kPageFaultUser));
    EXPECT_TRUE(vmem.last_page_fault()->is_not_present());
    EXPECT_TRUE(vmem.last_page_fault()->is_write());
    EXPECT_TRUE(vmem.last_page_fault()->is_user());

    // User Fetch with Not Present page:
    // P=0, W=0, U=1 (bit 2), I=1 (bit 4) -> error code 0b10100 = 20
    auto res_np_fetch = vmem.Fetch8(0x00400000);
    EXPECT_FALSE(res_np_fetch.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_EQ(vmem.last_page_fault()->error_code, (kPageFaultUser | kPageFaultInstruction));
    EXPECT_TRUE(vmem.last_page_fault()->is_instruction_fetch());
}

TEST_CASE(TestPdePteWalkAndOffsets) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x0000, 128 * 1024, ram, 0).has_value());

    VirtualMemory vmem(space);

    // Set up Page Directory at 0x1000
    const GuestAddr dir_phys = 0x1000;
    // Set up Page Table at 0x2000
    const GuestAddr tbl_phys = 0x2000;
    // Data page at 0x3000
    const GuestAddr data_phys = 0x3000;

    // Linear address: 0x00405123
    // Directory index: (0x00405123 >> 22) = 1
    // Table index: (0x00405123 >> 12) & 0x3FF = 5
    // Offset: 0x123
    const u32 pde_idx = (0x00405123 >> 22) & 0x3FF;
    const u32 pte_idx = (0x00405123 >> 12) & 0x3FF;
    EXPECT_EQ(pde_idx, 1u);
    EXPECT_EQ(pte_idx, 5u);

    // PDE[1]: points to tbl_phys, present (1), writable (2), user (4) = 7
    EXPECT_TRUE(
        space.Write32(dir_phys + (pde_idx * 4), tbl_phys | kPagePresent | kPageWritable | kPageUser)
            .has_value());

    // PTE[5]: points to data_phys, present (1), writable (2), user (4) = 7
    EXPECT_TRUE(
        space
            .Write32(tbl_phys + (pte_idx * 4), data_phys | kPagePresent | kPageWritable | kPageUser)
            .has_value());

    // Write a test value directly to data_phys + 0x123
    EXPECT_TRUE(space.Write32(data_phys + 0x123, 0x12345678).has_value());

    // Enable paging
    vmem.SetCr3(dir_phys);
    vmem.SetCr0(kCr0Paging);
    vmem.SetCpl(0);

    // Read virtual address 0x00405123
    auto val = vmem.Read32(0x00405123);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0x12345678u);

    // Translate preserves 12-bit offset
    auto trans = vmem.Translate(0x00405123, VirtualAccessType::Read);
    EXPECT_TRUE(trans.has_value());
    EXPECT_EQ(trans->physical_address, data_phys + 0x123);
}

TEST_CASE(TestPermissionsSupervisorUserAndWriteProtect) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x0000, 128 * 1024, ram, 0).has_value());

    VirtualMemory vmem(space);
    const GuestAddr dir_phys = 0x1000;
    const GuestAddr tbl_phys = 0x2000;
    const GuestAddr data_phys = 0x3000;

    vmem.SetCr3(dir_phys);
    vmem.SetCr0(kCr0Paging | kCr0WriteProtect); // Paging ON, WP ON

    // Read-only Supervisor page (P=1, W=0, U=0)
    EXPECT_TRUE(space.Write32(dir_phys, tbl_phys | kPagePresent).has_value());
    EXPECT_TRUE(space.Write32(tbl_phys, data_phys | kPagePresent).has_value()); // U/S = 0, R/W = 0

    // 1. Supervisor (CPL 0) Read -> Allowed
    vmem.SetCpl(0);
    auto s_read = vmem.Read32(0x00000000);
    EXPECT_TRUE(s_read.has_value());

    // 2. Supervisor (CPL 0) Write with WP=1 -> Protection Violation
    auto s_write = vmem.Write32(0x00000000, 0x99);
    EXPECT_FALSE(s_write.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_TRUE(vmem.last_page_fault()->is_protection_violation());
    EXPECT_TRUE(vmem.last_page_fault()->is_write());
    EXPECT_FALSE(vmem.last_page_fault()->is_user());

    // 3. User (CPL 3) Read to supervisor-only page -> Protection Violation
    vmem.InvalidateAll();
    vmem.SetCpl(3);
    auto u_read = vmem.Read32(0x00000000);
    EXPECT_FALSE(u_read.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_TRUE(vmem.last_page_fault()->is_protection_violation());
    EXPECT_TRUE(vmem.last_page_fault()->is_user());

    // Now make page user-accessible and read-only: (P=1, W=0, U=1)
    EXPECT_TRUE(space.Write32(dir_phys, tbl_phys | kPagePresent | kPageUser).has_value());
    EXPECT_TRUE(space.Write32(tbl_phys, data_phys | kPagePresent | kPageUser).has_value());
    vmem.InvalidateAll();

    // 4. User Read -> Allowed
    auto u_read2 = vmem.Read32(0x00000000);
    EXPECT_TRUE(u_read2.has_value());

    // 5. User Write -> Protection Violation
    auto u_write = vmem.Write32(0x00000000, 0x1234);
    EXPECT_FALSE(u_write.has_value());
    EXPECT_TRUE(vmem.last_page_fault().has_value());
    EXPECT_TRUE(vmem.last_page_fault()->is_protection_violation());
    EXPECT_TRUE(vmem.last_page_fault()->is_write());
    EXPECT_TRUE(vmem.last_page_fault()->is_user());
}

TEST_CASE(TestTlbInvalidationAndRemapping) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x0000, 128 * 1024, ram, 0).has_value());

    VirtualMemory vmem(space);
    const GuestAddr dir_phys = 0x1000;
    const GuestAddr tbl_phys = 0x2000;
    const GuestAddr data1_phys = 0x3000;
    const GuestAddr data2_phys = 0x4000;

    EXPECT_TRUE(
        space.Write32(dir_phys, tbl_phys | kPagePresent | kPageWritable | kPageUser).has_value());
    EXPECT_TRUE(
        space.Write32(tbl_phys, data1_phys | kPagePresent | kPageWritable | kPageUser).has_value());

    EXPECT_TRUE(space.Write32(data1_phys, 0x11111111).has_value());
    EXPECT_TRUE(space.Write32(data2_phys, 0x22222222).has_value());

    vmem.SetCr3(dir_phys);
    vmem.SetCr0(kCr0Paging);
    vmem.SetCpl(0);

    // Initial read populates TLB
    auto r1 = vmem.Read32(0x00000000);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, 0x11111111u);

    // Remap PTE in memory to data2_phys
    EXPECT_TRUE(
        space.Write32(tbl_phys, data2_phys | kPagePresent | kPageWritable | kPageUser).has_value());

    // Without invalidation, TLB returns old mapping
    auto r_cached = vmem.Read32(0x00000000);
    EXPECT_TRUE(r_cached.has_value());
    EXPECT_EQ(*r_cached, 0x11111111u);

    // After InvalidateTranslation, reads new mapping
    vmem.InvalidateTranslation(0x00000000);
    auto r2 = vmem.Read32(0x00000000);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, 0x22222222u);
}

TEST_CASE(TestCpuProgramThroughVirtualMemory) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x0000, 128 * 1024, ram, 0).has_value());

    VirtualMemory vmem(space);
    const GuestAddr dir_phys = 0x1000;
    const GuestAddr tbl_phys = 0x2000;
    const GuestAddr code_phys = 0x3000;
    const GuestAddr data_phys = 0x4000;

    // Virtual page 0x00010000 -> code_phys (R-X)
    // Virtual page 0x00020000 -> data_phys (RW)
    // Directory entry for 0x00000000..0x003FFFFF is index 0
    EXPECT_TRUE(space.Write32(dir_phys + 0, tbl_phys | kPagePresent | kPageWritable | kPageUser)
                    .has_value());

    // Table index 16 (0x00010000 >> 12 = 16) -> code_phys (Present, User, ReadOnly)
    EXPECT_TRUE(
        space.Write32(tbl_phys + (16 * 4), code_phys | kPagePresent | kPageUser).has_value());
    // Table index 32 (0x00020000 >> 12 = 32) -> data_phys (Present, User, Writable)
    EXPECT_TRUE(
        space.Write32(tbl_phys + (32 * 4), data_phys | kPagePresent | kPageWritable | kPageUser)
            .has_value());

    // Write synthetic program to code_phys:
    // MOV EAX, 0x42
    // MOV [0x00020004], EAX
    // ADD EAX, 8
    // HLT
    const std::vector<u8> code = {
        0xB8, 0x42, 0x00, 0x00, 0x00, // MOV EAX, 0x42
        0xA3, 0x04, 0x00, 0x02, 0x00, // MOV [0x00020004], EAX
        0x05, 0x08, 0x00, 0x00, 0x00, // ADD EAX, 8
        0xF4                          // HLT
    };
    EXPECT_TRUE(space.WriteBytes(code_phys, code).has_value());

    vmem.SetCr3(dir_phys);
    vmem.SetCr0(kCr0Paging | kCr0WriteProtect);
    vmem.SetCpl(0);

    Cpu cpu;
    cpu.context().eip = 0x00010000; // Virtual entry point!

    auto outcome = cpu.RunWithBudget(vmem, 100, 100);
    EXPECT_EQ(outcome.status, RunStatus::Halted);
    EXPECT_EQ(outcome.instructions_executed, 4u);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x4Au); // 0x42 + 8 = 0x4A

    // Verify written data in virtual data page (which is at data_phys + 4)
    auto stored_val = vmem.Read32(0x00020004);
    EXPECT_TRUE(stored_val.has_value());
    EXPECT_EQ(*stored_val, 0x42u);

    // Verify written data physically
    auto phys_stored = space.Read32(data_phys + 4);
    EXPECT_TRUE(phys_stored.has_value());
    EXPECT_EQ(*phys_stored, 0x42u);
}

int main() {
    return xblob::testing::RunAllTests();
}
