#include "tests/fixtures/synthetic_media.hpp"
#include "tests/test_framework.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/loader/xbe_loader.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/ram.hpp"

using namespace xblob;
using namespace xblob::loader;
using namespace xblob::memory;

TEST_CASE(TestXbeLoaderPlanningIsPureDataWithoutMemoryMutation) {
    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 2);
    MemoryByteSource source(xbe_bytes);

    auto plan_res = XbeLoader::Plan(source);
    EXPECT_TRUE(plan_res.has_value());

    const auto& plan = *plan_res;
    EXPECT_EQ(plan.base_address, 0x00010000u);
    EXPECT_EQ(plan.headers_size, 0x1000u);
    EXPECT_EQ(plan.image_size, 0x5000u);
    EXPECT_EQ(plan.entry_point, 0x00011000u);
    EXPECT_EQ(plan.sections.size(), 2u);

    // Section 0: .text (Executable)
    EXPECT_EQ(plan.sections[0].name, ".text");
    EXPECT_EQ(plan.sections[0].virtual_address, 0x00011000u);
    EXPECT_EQ(plan.sections[0].virtual_size, 0x1000u);
    EXPECT_EQ(plan.sections[0].raw_address, 0x1000u);
    EXPECT_EQ(plan.sections[0].raw_size, 0x1000u);
    EXPECT_EQ(plan.sections[0].zero_fill_size, 0u);
    EXPECT_TRUE((plan.sections[0].permissions & MemoryPermission::Execute) !=
                MemoryPermission::None);
    EXPECT_TRUE((plan.sections[0].permissions & MemoryPermission::Read) != MemoryPermission::None);
    EXPECT_FALSE((plan.sections[0].permissions & MemoryPermission::Write) !=
                 MemoryPermission::None);

    // Section 1: .rdata (Preload/ReadOnly)
    EXPECT_EQ(plan.sections[1].name, ".rdata");
    EXPECT_EQ(plan.sections[1].virtual_address, 0x00012000u);
    EXPECT_TRUE((plan.sections[1].permissions & MemoryPermission::Read) != MemoryPermission::None);
    EXPECT_FALSE((plan.sections[1].permissions & MemoryPermission::Execute) !=
                 MemoryPermission::None);
    EXPECT_FALSE((plan.sections[1].permissions & MemoryPermission::Write) !=
                 MemoryPermission::None);
}

TEST_CASE(TestXbeLoaderRejectsMaliciousAndMalformedImages) {
    // 1. TLS address != 0 rejected
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        // Write tls_address at 0x12C
        bytes[0x12C] = 0x00;
        bytes[0x12D] = 0x50;
        bytes[0x12E] = 0x01;
        bytes[0x12F] = 0x00;
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::UnsupportedFeature);
    }

    // 2. Raw size > virtual size rejected
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        // Section 0 virtual size at 0x408 = 0x800, raw size at 0x410 = 0x1000
        bytes[0x408] = 0x00;
        bytes[0x409] = 0x08; // virtual_size = 0x800
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::OutOfBounds);
    }

    // 3. Raw section data truncated / past file end
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        bytes.resize(0x1500); // Section 0 raw_address 0x1000 + raw_size 0x1000 = 0x2000 > 0x1500
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::OutOfBounds);
    }

    // 4. Section overlapping header
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        // Base is 0x00010000, headers_size is 0x1000 (headers occupy 0x10000..0x11000)
        // Set section virtual address to 0x10800
        bytes[0x404] = 0x00;
        bytes[0x405] = 0x08; // 0x00010800
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::RegionOverlap);
    }

    // 5. Entry point outside image
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        // Image size 0x5000 (end = 0x15000). Set entry point to 0x00020000
        bytes[0x128] = 0x00;
        bytes[0x129] = 0x00;
        bytes[0x12A] = 0x02;
        bytes[0x12B] = 0x00;
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::OutOfBounds);
    }

    // 6. Unsupported section flags (e.g. unknown bit 0x80)
    {
        auto bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
        bytes[0x400] = 0x84; // Flag with high unsupported bit
        MemoryByteSource src(bytes);
        auto plan = XbeLoader::Plan(src);
        EXPECT_FALSE(plan.has_value());
        EXPECT_EQ(plan.error().code, ErrorCode::UnsupportedFeature);
    }
}

TEST_CASE(TestXbeLoaderApplyAndZeroFill) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;

    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x00000000, kRamSizeRetail, ram, 0).has_value());

    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 2);
    // Put distinctive bytes in section 0
    xbe_bytes[0x1000] = 0x90; // NOP
    xbe_bytes[0x1001] = 0xF4; // HLT

    MemoryByteSource source(xbe_bytes);
    auto plan_res = XbeLoader::Load(source, space);
    EXPECT_TRUE(plan_res.has_value());

    // Verify headers copied to 0x00010000
    auto magic0 = space.Read8(0x00010000);
    EXPECT_TRUE(magic0.has_value());
    EXPECT_EQ(*magic0, static_cast<u8>('X'));

    // Verify section 0 copied to 0x00011000
    auto b0 = space.Read8(0x00011000);
    EXPECT_TRUE(b0.has_value());
    EXPECT_EQ(*b0, 0x90u);

    auto b1 = space.Read8(0x00011001);
    EXPECT_TRUE(b1.has_value());
    EXPECT_EQ(*b1, 0xF4u);
}

TEST_CASE(TestXbeLoaderTransactionalRollbackOnLateFailure) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    auto& ram = *ram_res;

    // Map RAM only for headers (0x00010000..0x00011000). Section 0 (0x00011000) will be unmapped!
    AddressSpace space;
    EXPECT_TRUE(space.MapRam(0x00010000, 0x1000, ram, 0).has_value());

    // Write a sentinel value into RAM at 0x00010000
    EXPECT_TRUE(space.Write32(0x00010000, 0xAABBCCDD).has_value());

    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 1);
    MemoryByteSource source(xbe_bytes);

    // Plan succeeds because planning does not know the address space mappings
    auto plan_res = XbeLoader::Plan(source);
    EXPECT_TRUE(plan_res.has_value());

    // Apply will fail when trying to backup/write section 0 which is unmapped!
    auto apply_res = XbeLoader::Apply(*plan_res, source, space);
    EXPECT_FALSE(apply_res.has_value());

    // Verify that sentinel value at 0x00010000 is perfectly preserved by rollback!
    auto restored_val = space.Read32(0x00010000);
    EXPECT_TRUE(restored_val.has_value());
    EXPECT_EQ(*restored_val, 0xAABBCCDDu);
}

TEST_CASE(TestXbeLoaderInitialContextGenerationNoCpuSideEffects) {
    auto xbe_bytes = testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 2);
    MemoryByteSource source(xbe_bytes);

    auto plan = XbeLoader::Plan(source);
    EXPECT_TRUE(plan.has_value());

    auto ctx = XbeLoader::CreateInitialContext(*plan, 0x00080000, 0x00010000);
    EXPECT_EQ(ctx.entry_point, 0x00011000u);
    EXPECT_EQ(ctx.base_address, 0x00010000u);
    EXPECT_EQ(ctx.image_size, 0x5000u);
    EXPECT_EQ(ctx.stack_top, 0x00080000u);
    EXPECT_EQ(ctx.stack_size, 0x00010000u);
    EXPECT_EQ(ctx.cpu_context.eip, 0x00011000u);
    EXPECT_EQ(ctx.cpu_context.GetGpr(cpu::Reg32::ESP), 0x00080000u);
    EXPECT_EQ(ctx.cpu_context.eflags, 0x00000002u);
}

int main() {
    return xblob::testing::RunAllTests();
}
