#include "tests/test_framework.hpp"
#include "xblob/bus/bus.hpp"
#include "xblob/bus/synthetic_register_bank.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/bus_adapter.hpp"

using namespace xblob;
using namespace xblob::bus;
using namespace xblob::memory;

TEST_CASE(TestBusRangeValidation) {
    Bus bus;
    auto dev1 = std::make_shared<SyntheticRegisterBank>(64);

    // Null device
    auto res_null = bus.MapDevice(0x1000, 0x100, nullptr);
    EXPECT_FALSE(res_null.has_value());
    EXPECT_EQ(res_null.error().code, ErrorCode::InvalidArgument);

    // Zero size
    auto res_zero = bus.MapDevice(0x1000, 0, dev1);
    EXPECT_FALSE(res_zero.has_value());
    EXPECT_EQ(res_zero.error().code, ErrorCode::InvalidArgument);

    // Integer overflow (base + size > 0x100000000)
    auto res_overflow = bus.MapDevice(0xFFFFFF00, 0x200, dev1);
    EXPECT_FALSE(res_overflow.has_value());
    EXPECT_EQ(res_overflow.error().code, ErrorCode::IntegerOverflow);

    // Valid map
    auto res_ok = bus.MapDevice(0x1000, 0x100, dev1);
    EXPECT_TRUE(res_ok.has_value());
    EXPECT_EQ(bus.mapping_count(), 1u);

    // Overlap: inside existing range
    auto dev2 = std::make_shared<SyntheticRegisterBank>(64);
    auto res_overlap1 = bus.MapDevice(0x1050, 0x20, dev2);
    EXPECT_FALSE(res_overlap1.has_value());
    EXPECT_EQ(res_overlap1.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(bus.mapping_count(), 1u); // Atomic preservation

    // Overlap: spans beginning
    auto res_overlap2 = bus.MapDevice(0x0F00, 0x150, dev2);
    EXPECT_FALSE(res_overlap2.has_value());
    EXPECT_EQ(res_overlap2.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(bus.mapping_count(), 1u);

    // Overlap: spans end
    auto res_overlap3 = bus.MapDevice(0x1080, 0x100, dev2);
    EXPECT_FALSE(res_overlap3.has_value());
    EXPECT_EQ(res_overlap3.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(bus.mapping_count(), 1u);

    // Non-overlapping adjacent map is OK
    auto res_adj = bus.MapDevice(0x1100, 0x100, dev2);
    EXPECT_TRUE(res_adj.has_value());
    EXPECT_EQ(bus.mapping_count(), 2u);
}

TEST_CASE(TestBusDispatchAndUnmapped) {
    Bus bus;
    auto dev = std::make_shared<SyntheticRegisterBank>(64);
    EXPECT_TRUE(bus.MapDevice(0xFD000000, 64, dev).has_value());

    // Unmapped address read/write
    auto read_unmapped = bus.Read(0xFC000000, BusAccessWidth::Dword);
    EXPECT_FALSE(read_unmapped.has_value());
    EXPECT_EQ(read_unmapped.error().code, ErrorCode::UnmappedAddress);

    auto write_unmapped = bus.Write(0xFC000000, BusAccessWidth::Dword, 0x12345678);
    EXPECT_FALSE(write_unmapped.has_value());
    EXPECT_EQ(write_unmapped.error().code, ErrorCode::UnmappedAddress);

    // Out of device range within mapped base
    auto read_oob = bus.Read(0xFD000000 + 64, BusAccessWidth::Byte);
    EXPECT_FALSE(read_oob.has_value());
    EXPECT_EQ(read_oob.error().code, ErrorCode::UnmappedAddress);

    // Valid write and read
    EXPECT_TRUE(bus.Write(0xFD000004, BusAccessWidth::Dword, 0xCAFEBABE).has_value());
    auto read_val = bus.Read(0xFD000004, BusAccessWidth::Dword);
    EXPECT_TRUE(read_val.has_value());
    EXPECT_EQ(*read_val, 0xCAFEBABEu);

    // Unmap device
    EXPECT_TRUE(bus.UnmapDevice(0xFD000000).has_value());
    EXPECT_EQ(bus.mapping_count(), 0u);

    auto read_after_unmap = bus.Read(0xFD000004, BusAccessWidth::Dword);
    EXPECT_FALSE(read_after_unmap.has_value());
    EXPECT_EQ(read_after_unmap.error().code, ErrorCode::UnmappedAddress);
}

TEST_CASE(TestSyntheticRegisterBankDeterministicLogging) {
    auto dev = std::make_shared<SyntheticRegisterBank>(64);

    EXPECT_EQ(dev->name(), "synthetic_register_bank");
    EXPECT_TRUE(dev->access_log().empty());

    // 8-bit, 16-bit, 32-bit round trips
    EXPECT_TRUE(dev->Write(0, BusAccessWidth::Byte, 0x42).has_value());
    EXPECT_TRUE(dev->Write(1, BusAccessWidth::Byte, 0x33).has_value());
    EXPECT_TRUE(dev->Write(2, BusAccessWidth::Word, 0x1234).has_value());

    auto r8 = dev->Read(0, BusAccessWidth::Byte);
    EXPECT_TRUE(r8.has_value());
    EXPECT_EQ(*r8, 0x42u);

    auto r16 = dev->Read(2, BusAccessWidth::Word);
    EXPECT_TRUE(r16.has_value());
    EXPECT_EQ(*r16, 0x1234u);

    auto r32 = dev->Read(0, BusAccessWidth::Dword);
    EXPECT_TRUE(r32.has_value());
    EXPECT_EQ(*r32, 0x12343342u);

    // Verify deterministic access log
    const auto& log = dev->access_log();
    EXPECT_EQ(log.size(), 6u);

    EXPECT_TRUE(log[0].is_write);
    EXPECT_EQ(log[0].offset, 0u);
    EXPECT_EQ(log[0].width, BusAccessWidth::Byte);
    EXPECT_EQ(log[0].value, 0x42u);

    EXPECT_TRUE(log[1].is_write);
    EXPECT_EQ(log[1].offset, 1u);
    EXPECT_EQ(log[1].width, BusAccessWidth::Byte);
    EXPECT_EQ(log[1].value, 0x33u);

    EXPECT_TRUE(log[2].is_write);
    EXPECT_EQ(log[2].offset, 2u);
    EXPECT_EQ(log[2].width, BusAccessWidth::Word);
    EXPECT_EQ(log[2].value, 0x1234u);

    EXPECT_FALSE(log[3].is_write);
    EXPECT_EQ(log[3].offset, 0u);
    EXPECT_EQ(log[3].width, BusAccessWidth::Byte);
    EXPECT_EQ(log[3].value, 0x42u);

    EXPECT_FALSE(log[4].is_write);
    EXPECT_EQ(log[4].offset, 2u);
    EXPECT_EQ(log[4].width, BusAccessWidth::Word);
    EXPECT_EQ(log[4].value, 0x1234u);

    EXPECT_FALSE(log[5].is_write);
    EXPECT_EQ(log[5].offset, 0u);
    EXPECT_EQ(log[5].width, BusAccessWidth::Dword);
    EXPECT_EQ(log[5].value, 0x12343342u);

    dev->ClearLog();
    EXPECT_EQ(dev->access_log().size(), 0u);
}

TEST_CASE(TestBusMmioAdapterIntegration) {
    Bus bus;
    auto dev = std::make_shared<SyntheticRegisterBank>(64);
    EXPECT_TRUE(bus.MapDevice(0xFD000000, 64, dev).has_value());

    AddressSpace space;
    EXPECT_TRUE(MapBusMmio(space, 0xFD000000, 64, bus).has_value());

    // Write through AddressSpace
    EXPECT_TRUE(space.Write32(0xFD000008, 0xDEADBEEF).has_value());
    EXPECT_TRUE(space.Write8(0xFD00000C, 0x77).has_value());
    EXPECT_TRUE(space.Write16(0xFD00000E, 0x99AA).has_value());

    // Read back through AddressSpace
    auto val32 = space.Read32(0xFD000008);
    EXPECT_TRUE(val32.has_value());
    EXPECT_EQ(*val32, 0xDEADBEEFu);

    auto val8 = space.Read8(0xFD00000C);
    EXPECT_TRUE(val8.has_value());
    EXPECT_EQ(*val8, 0x77u);

    auto val16 = space.Read16(0xFD00000E);
    EXPECT_TRUE(val16.has_value());
    EXPECT_EQ(*val16, 0x99AAu);

    // Verify access log on register bank
    EXPECT_EQ(dev->access_log().size(), 6u);
}

int main() {
    return xblob::testing::RunAllTests();
}
