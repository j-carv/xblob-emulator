#include "tests/test_framework.hpp"
#include "xblob/bus/bus.hpp"
#include "xblob/bus/synthetic_register_bank.hpp"
#include "xblob/pci/pci_bar.hpp"
#include "xblob/pci/pci_bus_bridge.hpp"
#include "xblob/pci/pci_config_header.hpp"
#include "xblob/pci/pci_device.hpp"
#include "xblob/pci/pci_registry.hpp"
#include "xblob/pci/pci_types.hpp"

using namespace xblob;
using namespace xblob::pci;
using namespace xblob::bus;

class TestMockPciDevice : public PciDevice {
public:
    TestMockPciDevice(PciBdf bdf, std::string name) : PciDevice(bdf), name_(std::move(name)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::string name_;
};

TEST_CASE(TestPciBdfBasics) {
    PciBdf bdf0{0, 0, 0};
    EXPECT_EQ(bdf0.ToString(), "00:00.0");
    EXPECT_EQ(bdf0.raw(), 0u);

    PciBdf bdf1{1, 5, 2};
    EXPECT_EQ(bdf1.ToString(), "01:05.2");
    EXPECT_EQ(bdf1.bus, 1u);
    EXPECT_EQ(bdf1.device, 5u);
    EXPECT_EQ(bdf1.function, 2u);

    EXPECT_TRUE(bdf0 < bdf1);
    EXPECT_FALSE(bdf1 < bdf0);
    EXPECT_TRUE(bdf0 == (PciBdf{0, 0, 0}));
}

TEST_CASE(TestPciConfigHeaderAccessAndAlignment) {
    PciConfigHeader header;
    header.SetVendorId(0x10DE);
    header.SetDeviceId(0x02A0);
    header.SetClassCodes(0x03, 0x00, 0x00);
    header.SetRevisionId(0xA1);

    // Vendor and Device ID
    auto v_res = header.Read(kPciVendorIdOffset, PciAccessWidth::Word);
    EXPECT_TRUE(v_res.has_value());
    EXPECT_EQ(*v_res, 0x10DEu);

    auto d_res = header.Read(kPciDeviceIdOffset, PciAccessWidth::Word);
    EXPECT_TRUE(d_res.has_value());
    EXPECT_EQ(*d_res, 0x02A0u);

    // Combined 32-bit read at offset 0
    auto vd_res = header.Read(kPciVendorIdOffset, PciAccessWidth::Dword);
    EXPECT_TRUE(vd_res.has_value());
    EXPECT_EQ(*vd_res, 0x02A010DEu);

    // Alignment verification
    auto unaligned_word = header.Read(1, PciAccessWidth::Word);
    EXPECT_FALSE(unaligned_word.has_value());
    EXPECT_EQ(unaligned_word.error().code, ErrorCode::InvalidArgument);

    auto unaligned_dword = header.Read(2, PciAccessWidth::Dword);
    EXPECT_FALSE(unaligned_dword.has_value());
    EXPECT_EQ(unaligned_dword.error().code, ErrorCode::InvalidArgument);

    // Bounds checking
    auto out_of_bounds = header.Read(255, PciAccessWidth::Word);
    EXPECT_FALSE(out_of_bounds.has_value());
    EXPECT_EQ(out_of_bounds.error().code, ErrorCode::OutOfBounds);

    // Read-only preservation: writing to vendor ID should be ignored
    auto wr_ro = header.Write(kPciVendorIdOffset, PciAccessWidth::Word, 0xCAFE);
    EXPECT_TRUE(wr_ro.has_value());
    auto v_check = header.Read(kPciVendorIdOffset, PciAccessWidth::Word);
    EXPECT_TRUE(v_check.has_value());
    EXPECT_EQ(*v_check, 0x10DEu);

    // Command register writable
    auto wr_cmd = header.Write(kPciCommandOffset, PciAccessWidth::Word, kPciCommandMemorySpace);
    EXPECT_TRUE(wr_cmd.has_value());
    EXPECT_TRUE(header.is_memory_space_enabled());
    auto rd_cmd = header.Read(kPciCommandOffset, PciAccessWidth::Word);
    EXPECT_TRUE(rd_cmd.has_value());
    EXPECT_EQ(*rd_cmd, static_cast<u32>(kPciCommandMemorySpace));
}

TEST_CASE(TestPciRegistryAndAbsentDevice) {
    PciRegistry registry;
    auto dev1 = std::make_shared<TestMockPciDevice>(PciBdf{0, 0, 0}, "NV2A GPU");
    dev1->config_header().SetVendorId(kPciVendorIdNvidia);
    dev1->config_header().SetDeviceId(kPciDeviceIdNv2a);
    dev1->config_header().SetClassCodes(kPciClassDisplay, kPciSubclassVga, kPciProgIfVga);
    dev1->config_header().SetRevisionId(kPciRevisionNv2a);

    EXPECT_TRUE(registry.RegisterDevice(dev1).has_value());
    EXPECT_EQ(registry.device_count(), 1u);

    // Duplicate BDF rejection
    auto dev_dup = std::make_shared<TestMockPciDevice>(PciBdf{0, 0, 0}, "Duplicate");
    auto dup_res = registry.RegisterDevice(dev_dup);
    EXPECT_FALSE(dup_res.has_value());
    EXPECT_EQ(dup_res.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(registry.device_count(), 1u);

    // Present device read
    auto r_pres = registry.ReadConfig(PciBdf{0, 0, 0}, kPciVendorIdOffset, PciAccessWidth::Word);
    EXPECT_TRUE(r_pres.has_value());
    EXPECT_EQ(*r_pres, 0x10DEu);

    // Absent device read returns all-ones
    auto r_abs8 = registry.ReadConfig(PciBdf{0, 1, 0}, 0x00, PciAccessWidth::Byte);
    EXPECT_TRUE(r_abs8.has_value());
    EXPECT_EQ(*r_abs8, 0xFFu);

    auto r_abs16 = registry.ReadConfig(PciBdf{0, 1, 0}, 0x00, PciAccessWidth::Word);
    EXPECT_TRUE(r_abs16.has_value());
    EXPECT_EQ(*r_abs16, 0xFFFFu);

    auto r_abs32 = registry.ReadConfig(PciBdf{0, 1, 0}, 0x00, PciAccessWidth::Dword);
    EXPECT_TRUE(r_abs32.has_value());
    EXPECT_EQ(*r_abs32, 0xFFFFFFFFu);

    // Absent device write succeeds silently
    auto w_abs = registry.WriteConfig(PciBdf{0, 1, 0}, 0x00, PciAccessWidth::Dword, 0x12345678);
    EXPECT_TRUE(w_abs.has_value());

    // Deterministic enumeration
    auto dev2 = std::make_shared<TestMockPciDevice>(PciBdf{1, 0, 0}, "NIC");
    auto dev3 = std::make_shared<TestMockPciDevice>(PciBdf{0, 2, 0}, "Audio");
    EXPECT_TRUE(registry.RegisterDevice(dev2).has_value());
    EXPECT_TRUE(registry.RegisterDevice(dev3).has_value());

    auto list = registry.EnumerateDevices();
    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0]->bdf(), (PciBdf{0, 0, 0}));
    EXPECT_EQ(list[1]->bdf(), (PciBdf{0, 2, 0}));
    EXPECT_EQ(list[2]->bdf(), (PciBdf{1, 0, 0}));
}

TEST_CASE(TestPciBarSizingAndTransactionalProgramming) {
    PciBar bar(0, 0x01000000); // 16 MB BAR
    EXPECT_TRUE(bar.is_configured());
    EXPECT_FALSE(bar.is_active());

    // 1. Probe sizing protocol
    EXPECT_TRUE(bar.Write(0xFFFFFFFFu).has_value());
    // Sizing response: ~(16MB - 1) & 0xFFFFFFF0 = 0xFF000000
    EXPECT_EQ(bar.Read(), 0xFF000000u);

    // 2. Valid programming
    EXPECT_TRUE(bar.Write(0xFD000000u).has_value());
    EXPECT_TRUE(bar.is_active());
    EXPECT_EQ(bar.base_address(), 0xFD000000u);
    EXPECT_EQ(bar.Read(), 0xFD000000u);

    // 3. Misaligned programming -> fails and rolls back
    auto misaligned_res = bar.Write(0xFD001000u);
    EXPECT_FALSE(misaligned_res.has_value());
    EXPECT_EQ(misaligned_res.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(bar.base_address(), 0xFD000000u); // Preserved previous value!

    // 4. Overflow programming -> fails and rolls back
    auto overflow_res = bar.Write(0xFF800000u);
    EXPECT_FALSE(overflow_res.has_value());
    EXPECT_EQ(overflow_res.error().code, ErrorCode::IntegerOverflow);
    EXPECT_EQ(bar.base_address(), 0xFD000000u); // Preserved!
}

TEST_CASE(TestPciBusBridgeIntegrationAndGating) {
    Bus bus;
    PciRegistry registry;

    auto dev = std::make_shared<TestMockPciDevice>(PciBdf{0, 0, 0}, "NV2A GPU");
    auto reg_bank = std::make_shared<SyntheticRegisterBank>(64);
    EXPECT_TRUE(
        dev->config_header().ConfigureBar(0, 0x01000000, false, false, reg_bank).has_value());

    EXPECT_TRUE(registry.RegisterDevice(dev).has_value());

    PciBusBridge bridge(bus, registry);

    // Program BAR0 via bridge
    EXPECT_TRUE(bridge.ProgramBar(PciBdf{0, 0, 0}, 0, 0xFD000000u).has_value());

    // With memory space disabled, MMIO should not reach device
    EXPECT_FALSE(dev->config_header().is_memory_space_enabled());
    auto read_disabled = bus.Read(0xFD000000u, BusAccessWidth::Dword);
    EXPECT_FALSE(read_disabled.has_value());
    EXPECT_EQ(read_disabled.error().code, ErrorCode::UnmappedAddress);

    // Enable memory space
    EXPECT_TRUE(bridge.SetMemorySpaceEnabled(PciBdf{0, 0, 0}, true).has_value());
    EXPECT_TRUE(dev->config_header().is_memory_space_enabled());

    // Now MMIO reaches the device
    EXPECT_TRUE(bus.Write(0xFD000000u, BusAccessWidth::Dword, 0x12345678u).has_value());
    auto read_enabled = bus.Read(0xFD000000u, BusAccessWidth::Dword);
    EXPECT_TRUE(read_enabled.has_value());
    EXPECT_EQ(*read_enabled, 0x12345678u);

    // Collision check: add a second device and try to map overlapping range
    auto dev2 = std::make_shared<TestMockPciDevice>(PciBdf{0, 1, 0}, "Audio");
    auto reg_bank2 = std::make_shared<SyntheticRegisterBank>(64);
    EXPECT_TRUE(
        dev2->config_header().ConfigureBar(0, 0x01000000, false, false, reg_bank2).has_value());
    EXPECT_TRUE(registry.RegisterDevice(dev2).has_value());

    // Try to program dev2 to exact same or overlapping address
    auto coll_res = bridge.ProgramBar(PciBdf{0, 1, 0}, 0, 0xFD000000u);
    EXPECT_FALSE(coll_res.has_value());
    EXPECT_EQ(coll_res.error().code, ErrorCode::RegionOverlap);

    // Dev1 mapping remains fully functional
    auto read_dev1 = bus.Read(0xFD000000u, BusAccessWidth::Dword);
    EXPECT_TRUE(read_dev1.has_value());
    EXPECT_EQ(*read_dev1, 0x12345678u);

    // Program dev2 to non-overlapping address
    EXPECT_TRUE(bridge.ProgramBar(PciBdf{0, 1, 0}, 0, 0xFE000000u).has_value());
    EXPECT_TRUE(bridge.SetMemorySpaceEnabled(PciBdf{0, 1, 0}, true).has_value());
    EXPECT_TRUE(bus.Write(0xFE000000u, BusAccessWidth::Dword, 0xDEADBEEFu).has_value());
    EXPECT_EQ(*bus.Read(0xFE000000u, BusAccessWidth::Dword), 0xDEADBEEFu);
    EXPECT_EQ(*bus.Read(0xFD000000u, BusAccessWidth::Dword), 0x12345678u);

    // Remap dev1 to another address
    EXPECT_TRUE(bridge.ProgramBar(PciBdf{0, 0, 0}, 0, 0xFC000000u).has_value());
    // Old address is unmapped
    EXPECT_FALSE(bus.Read(0xFD000000u, BusAccessWidth::Dword).has_value());
    // New address is mapped
    EXPECT_TRUE(bus.Read(0xFC000000u, BusAccessWidth::Dword).has_value());
}

int main() {
    return xblob::testing::RunAllTests();
}
