#include "tests/test_framework.hpp"
#include "tests/unit/test_usb_fixtures.hpp"
#include "xblob/usb/ohci_controller.hpp"
#include "xblob/usb/ohci_registers.hpp"
#include "xblob/usb/usb_device.hpp"

#include <memory>

using namespace xblob;
using namespace xblob::usb;
using namespace xblob::testing;

class MockSimpleDevice final : public UsbDevice {
public:
    explicit MockSimpleDevice(u8 address = 1) : address_(address) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "MockDevice"; }
    [[nodiscard]] u8 address() const noexcept override { return address_; }
    void set_address(u8 address) noexcept override { address_ = address; }
    [[nodiscard]] UsbSpeed speed() const noexcept override { return UsbSpeed::FullSpeed; }
    [[nodiscard]] bool is_connected() const noexcept override { return connected_; }

    void Reset() noexcept override { reset_count++; }

    [[nodiscard]] UsbTransferResult
    HandleControlTransfer(const UsbSetupPacket& setup, [[maybe_unused]] ByteSpan out_payload,
                          MutableByteSpan in_buffer) noexcept override {
        control_requests_count++;
        if (setup.request == 0x42) {
            if (!in_buffer.empty()) {
                in_buffer[0] = 0xAA;
                return UsbTransferResult{UsbTransferStatus::Success, 1, 0};
            }
            return UsbTransferResult{UsbTransferStatus::Success, 0, 0};
        }
        return UsbTransferResult{UsbTransferStatus::Stalled, 0, 4};
    }

    [[nodiscard]] UsbTransferResult
    HandleInterruptTransfer([[maybe_unused]] u8 endpoint_address,
                            MutableByteSpan in_buffer) noexcept override {
        interrupt_polls_count++;
        if (!in_buffer.empty()) {
            in_buffer[0] = 0x55;
            return UsbTransferResult{UsbTransferStatus::Success, 1, 0};
        }
        return UsbTransferResult{UsbTransferStatus::Success, 0, 0};
    }

    bool connected_{true};
    u8 address_{1};
    u32 reset_count{0};
    u32 control_requests_count{0};
    u32 interrupt_polls_count{0};
};

TEST_CASE(TestOhciRegisterDefaultsAndReset) {
    OhciController hc;
    auto rev = hc.ReadRegister(ohci::kRegHcRevision);
    EXPECT_TRUE(rev.has_value());
    EXPECT_EQ(*rev, ohci::kDefaultRevision);

    EXPECT_EQ(hc.control(), 0U);
    EXPECT_EQ(hc.command_status(), 0U);
    EXPECT_EQ(hc.interrupt_status(), 0U);
    EXPECT_EQ(hc.interrupt_enable(), 0U);
    EXPECT_FALSE(hc.IsIrqAsserted());

    // Trigger host controller reset
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational).has_value());
    EXPECT_EQ(hc.control(), ohci::control::kStateUsbOperational);

    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcCommandStatus, ohci::command_status::kHostControllerReset)
            .has_value());
    EXPECT_EQ(hc.control(), ohci::control::kStateUsbReset);
}

TEST_CASE(TestOhciInterruptW1CAndMasterEnable) {
    OhciController hc;

    // Writing 1 to HcInterruptEnable sets bits
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcInterruptEnable,
                         ohci::interrupt::kStartOfFrame | ohci::interrupt::kWritebackDoneHead)
            .has_value());
    EXPECT_EQ(hc.interrupt_enable(),
              ohci::interrupt::kStartOfFrame | ohci::interrupt::kWritebackDoneHead);

    // Synthetic memory for frame tick
    SyntheticUsbMemory mem;
    EXPECT_TRUE(hc.ProcessFrame(mem).has_value());

    // StartOfFrame bit should be set in status
    EXPECT_EQ(hc.interrupt_status() & ohci::interrupt::kStartOfFrame,
              ohci::interrupt::kStartOfFrame);

    // Master enable is not yet set, so IRQ must NOT be asserted
    EXPECT_FALSE(hc.IsIrqAsserted());

    // Enable Master Interrupt
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcInterruptEnable, ohci::interrupt::kMasterInterruptEnable)
            .has_value());
    EXPECT_TRUE(hc.IsIrqAsserted());

    // W1C: acknowledge StartOfFrame by writing 1 to HcInterruptStatus
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcInterruptStatus, ohci::interrupt::kStartOfFrame).has_value());
    EXPECT_EQ(hc.interrupt_status() & ohci::interrupt::kStartOfFrame, 0U);
    EXPECT_FALSE(hc.IsIrqAsserted());

    // Writing to HcInterruptDisable clears enable bit
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcInterruptDisable, ohci::interrupt::kWritebackDoneHead)
                    .has_value());
    EXPECT_EQ(hc.interrupt_enable() & ohci::interrupt::kWritebackDoneHead, 0U);
}

TEST_CASE(TestOhciRootHubPortStatusAndW1C) {
    OhciController hc;
    auto dev = std::make_shared<MockSimpleDevice>(1);

    // Initial port status should be powered
    u32 p0 = *hc.ReadRegister(ohci::kRegHcRhPortStatusBase);
    EXPECT_EQ(p0 & ohci::port_status::kPortPowerStatus, ohci::port_status::kPortPowerStatus);
    EXPECT_EQ(p0 & ohci::port_status::kCurrentConnectStatus, 0U);

    // Attach device
    EXPECT_TRUE(hc.AttachDevice(0, dev).has_value());
    p0 = *hc.ReadRegister(ohci::kRegHcRhPortStatusBase);
    EXPECT_EQ(p0 & ohci::port_status::kCurrentConnectStatus,
              ohci::port_status::kCurrentConnectStatus);
    EXPECT_EQ(p0 & ohci::port_status::kConnectStatusChange,
              ohci::port_status::kConnectStatusChange);

    // W1C clear connect status change
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcRhPortStatusBase, ohci::port_status::kConnectStatusChange)
            .has_value());
    p0 = *hc.ReadRegister(ohci::kRegHcRhPortStatusBase);
    EXPECT_EQ(p0 & ohci::port_status::kConnectStatusChange, 0U);

    // Reset port
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcRhPortStatusBase, ohci::port_status::kPortResetStatus)
                    .has_value());
    p0 = *hc.ReadRegister(ohci::kRegHcRhPortStatusBase);
    EXPECT_EQ(p0 & ohci::port_status::kPortEnableStatus, ohci::port_status::kPortEnableStatus);
    EXPECT_EQ(p0 & ohci::port_status::kPortResetStatusChange,
              ohci::port_status::kPortResetStatusChange);
    EXPECT_EQ(dev->reset_count, 1U);

    // Detach device
    EXPECT_TRUE(hc.DetachDevice(0).has_value());
    p0 = *hc.ReadRegister(ohci::kRegHcRhPortStatusBase);
    EXPECT_EQ(p0 & ohci::port_status::kCurrentConnectStatus, 0U);
    EXPECT_EQ(p0 & ohci::port_status::kPortEnableStatus, 0U);
}

TEST_CASE(TestOhciTraversalBoundedAndCycleDetection) {
    OhciController hc;
    SyntheticUsbMemory mem;
    auto dev = std::make_shared<MockSimpleDevice>(1);
    EXPECT_TRUE(hc.AttachDevice(0, dev).has_value());

    // Create a cyclic ED list (ED0 points to ED1, ED1 points to ED0)
    GuestAddr ed0_addr = mem.AllocateAligned(16, 16);
    GuestAddr ed1_addr = mem.AllocateAligned(16, 16);

    ohci::EndpointDescriptor ed0;
    ed0.flags = 1 | (0 << 7) | (2 << 11) | (64 << 16); // Addr 1, EP 0, IN
    ed0.next_ed = ed1_addr;
    ed0.head_p = 0;
    ed0.tail_p = 0;

    ohci::EndpointDescriptor ed1;
    ed1.flags = 1 | (0 << 7) | (2 << 11) | (64 << 16);
    ed1.next_ed = ed0_addr; // Forms cycle!
    ed1.head_p = 0;
    ed1.tail_p = 0;

    (void)mem.Write32(ed0_addr, ed0.flags);
    (void)mem.Write32(ed0_addr + 4, ed0.tail_p);
    (void)mem.Write32(ed0_addr + 8, ed0.head_p);
    (void)mem.Write32(ed0_addr + 12, ed0.next_ed);

    (void)mem.Write32(ed1_addr, ed1.flags);
    (void)mem.Write32(ed1_addr + 4, ed1.tail_p);
    (void)mem.Write32(ed1_addr + 8, ed1.head_p);
    (void)mem.Write32(ed1_addr + 12, ed1.next_ed);

    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kControlListEnable)
                    .has_value());
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControlHeadED, ed0_addr).has_value());

    // Frame process should detect cycle and return structured error without infinite loop
    auto f_res = hc.ProcessFrame(mem);
    EXPECT_FALSE(f_res.has_value());
    EXPECT_EQ(f_res.error().code, ErrorCode::LimitReached);
}

TEST_CASE(TestOhciTransactionalCommitOnFault) {
    OhciController hc;
    SyntheticUsbMemory mem;
    auto dev = std::make_shared<MockSimpleDevice>(1);
    EXPECT_TRUE(hc.AttachDevice(0, dev).has_value());

    // Set memory fault on a destination buffer
    GuestAddr buffer_addr = 0x8000;
    mem.SetFaultRange(buffer_addr, buffer_addr + 0x1000);

    GuestAddr dummy_td = mem.AllocateAligned(16, 16);

    ohci::GeneralTransferDescriptor td;
    td.flags = (2 << 19); // IN PID
    td.cbp = buffer_addr; // Fault range!
    td.next_td = dummy_td;
    td.be = buffer_addr + 31;
    GuestAddr td_addr = mem.WriteTransferDescriptor(td);

    ohci::EndpointDescriptor ed;
    ed.flags = 1 | (1 << 7) | (2 << 11) | (32 << 16); // Addr 1, EP 1, IN
    ed.head_p = td_addr;
    ed.tail_p = dummy_td;
    ed.next_ed = 0;
    GuestAddr ed_addr = mem.WriteEndpointDescriptor(ed);

    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kPeriodicListEnable)
                    .has_value());
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcPeriodCurrentED, ed_addr).has_value());

    auto res = hc.ProcessFrame(mem);
    EXPECT_TRUE(res.has_value());

    // ED should be halted and HeadP preserved on fault without corrupting system
    auto halted_ed = mem.Read32(ed_addr + 8);
    EXPECT_TRUE(halted_ed.has_value());
    EXPECT_TRUE((*halted_ed & 0x01) != 0); // Halted bit set
}

TEST_CASE(TestOhciCyclicTdListDetection) {
    OhciController hc;
    SyntheticUsbMemory mem;
    auto dev = std::make_shared<MockSimpleDevice>(1);
    EXPECT_TRUE(hc.AttachDevice(0, dev).has_value());

    // Create cyclic TD loop (TD0 points to TD1, TD1 points to TD0)
    GuestAddr td0_addr = mem.AllocateAligned(16, 16);
    GuestAddr td1_addr = mem.AllocateAligned(16, 16);

    ohci::GeneralTransferDescriptor td0;
    td0.flags = (2 << 19); // IN PID
    td0.cbp = 0x5000;
    td0.next_td = td1_addr;
    td0.be = 0x500F;

    ohci::GeneralTransferDescriptor td1;
    td1.flags = (2 << 19); // IN PID
    td1.cbp = 0x5010;
    td1.next_td = td0_addr; // Cycle!
    td1.be = 0x501F;

    (void)mem.Write32(td0_addr, td0.flags);
    (void)mem.Write32(td0_addr + 4, td0.cbp);
    (void)mem.Write32(td0_addr + 8, td0.next_td);
    (void)mem.Write32(td0_addr + 12, td0.be);

    (void)mem.Write32(td1_addr, td1.flags);
    (void)mem.Write32(td1_addr + 4, td1.cbp);
    (void)mem.Write32(td1_addr + 8, td1.next_td);
    (void)mem.Write32(td1_addr + 12, td1.be);

    ohci::EndpointDescriptor ed;
    ed.flags = 1 | (1 << 7) | (2 << 11) | (32 << 16);
    ed.head_p = td0_addr;
    ed.tail_p = 0; // Tail is 0, so loop is entered
    ed.next_ed = 0;
    GuestAddr ed_addr = mem.WriteEndpointDescriptor(ed);

    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kPeriodicListEnable)
                    .has_value());
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcPeriodCurrentED, ed_addr).has_value());

    // Frame process should detect TD cycle and return structured limit error
    auto res = hc.ProcessFrame(mem);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ErrorCode::LimitReached);
}

TEST_CASE(TestOhciUnalignedEdAndTd) {
    OhciController hc;
    SyntheticUsbMemory mem;

    // Unaligned ED (address not divisible by 16)
    GuestAddr unaligned_ed = 0x1004;
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kControlListEnable)
                    .has_value());
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControlHeadED, unaligned_ed).has_value());

    auto res = hc.ProcessFrame(mem);
    // Unaligned address masked out by register write (OHCI spec: bits 0..3 are 0)
    EXPECT_TRUE(res.has_value());
}

TEST_CASE(TestOhciSkipAndHaltedEdHandling) {
    OhciController hc;
    SyntheticUsbMemory mem;
    auto dev = std::make_shared<MockSimpleDevice>(1);
    EXPECT_TRUE(hc.AttachDevice(0, dev).has_value());

    // ED 0 has Skip bit set
    GuestAddr dummy_tail = mem.AllocateAligned(16, 16);
    GuestAddr td_addr = mem.AllocateAligned(16, 16);

    ohci::GeneralTransferDescriptor td;
    td.flags = (2 << 19);
    td.cbp = 0x6000;
    td.next_td = dummy_tail;
    td.be = 0x600F;
    (void)mem.WriteTransferDescriptor(td);

    ohci::EndpointDescriptor ed0;
    ed0.flags = 1 | (1 << 7) | (2 << 11) | (1 << 14) | (32 << 16); // Bit 14 is Skip!
    ed0.head_p = td_addr;
    ed0.tail_p = dummy_tail;
    ed0.next_ed = 0;
    GuestAddr ed0_addr = mem.WriteEndpointDescriptor(ed0);

    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kPeriodicListEnable)
                    .has_value());
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcPeriodCurrentED, ed0_addr).has_value());

    auto res = hc.ProcessFrame(mem);
    EXPECT_TRUE(res.has_value());

    // Because Skip was set, TD must NOT have been executed
    EXPECT_EQ(dev->interrupt_polls_count, 0U);
}

int main() {
    return testing::RunAllTests();
}
