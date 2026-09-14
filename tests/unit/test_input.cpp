#include "tests/test_framework.hpp"
#include "tests/unit/test_usb_fixtures.hpp"
#include "xblob/input/input_types.hpp"
#include "xblob/input/xid_controller.hpp"
#include "xblob/usb/ohci_controller.hpp"
#include "xblob/usb/ohci_registers.hpp"

#include <vector>

using namespace xblob;
using namespace xblob::input;
using namespace xblob::usb;
using namespace xblob::testing;

TEST_CASE(TestHostInputSnapshotSequenceValidation) {
    XidController controller;
    EXPECT_EQ(controller.current_sequence(), 0ULL);

    HostInputSnapshot s1;
    s1.sequence = 10;
    s1.digital_buttons = buttons::kStart | buttons::kDPadUp;
    EXPECT_TRUE(controller.SubmitSnapshot(s1));
    EXPECT_EQ(controller.current_sequence(), 10ULL);

    // Stale snapshot with lower sequence must be rejected
    HostInputSnapshot s_stale;
    s_stale.sequence = 9;
    s_stale.digital_buttons = buttons::kBack;
    EXPECT_FALSE(controller.SubmitSnapshot(s_stale));
    EXPECT_EQ(controller.current_sequence(), 10ULL);

    // Duplicate sequence must also be rejected
    HostInputSnapshot s_dup;
    s_dup.sequence = 10;
    EXPECT_FALSE(controller.SubmitSnapshot(s_dup));
    EXPECT_EQ(controller.current_sequence(), 10ULL);

    // Strictly greater sequence is accepted
    HostInputSnapshot s2;
    s2.sequence = 11;
    s2.button_a = 255;
    EXPECT_TRUE(controller.SubmitSnapshot(s2));
    EXPECT_EQ(controller.current_sequence(), 11ULL);
}

TEST_CASE(TestXidReportSerializationAndClamping) {
    HostInputSnapshot snap;
    snap.sequence = 1;
    snap.digital_buttons = buttons::kDPadLeft | buttons::kRightThumb;
    snap.button_a = 200;
    snap.button_b = 50;
    snap.trigger_left = 128;
    snap.trigger_right = 250;
    snap.thumb_lx = -32768;
    snap.thumb_ly = 32767;
    snap.thumb_rx = 1000;
    snap.thumb_ry = -2000;

    snap.ClampAndNormalize();

    auto report = XidGamepadReport::FromSnapshot(snap);
    std::vector<u8> buffer(kXidGamepadReportSize, 0);
    EXPECT_TRUE(report.Serialize(MutableByteSpan{buffer.data(), buffer.size()}).has_value());

    EXPECT_EQ(buffer[0], 0x00); // Report ID
    EXPECT_EQ(buffer[1], 0x14); // Length 20
    EXPECT_EQ(buffer[2], buttons::kDPadLeft | buttons::kRightThumb);
    EXPECT_EQ(buffer[3], 0x00);
    EXPECT_EQ(buffer[4], 200);  // A
    EXPECT_EQ(buffer[5], 50);   // B
    EXPECT_EQ(buffer[10], 128); // Left trigger
    EXPECT_EQ(buffer[11], 250); // Right trigger

    // Check little-endian decoding
    auto deserialized = XidGamepadReport::Deserialize(ByteSpan{buffer.data(), buffer.size()});
    EXPECT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->thumb_lx, -32768);
    EXPECT_EQ(deserialized->thumb_ly, 32767);
    EXPECT_EQ(deserialized->thumb_rx, 1000);
    EXPECT_EQ(deserialized->thumb_ry, -2000);
}

TEST_CASE(TestXidDescriptorsAndControlAllowlist) {
    XidController controller;
    std::vector<u8> in_buf(64, 0);

    // 1. Get Device Descriptor
    UsbSetupPacket get_dev_desc{0x80, requests::kGetDescriptor, 0x0100, 0, 18};
    auto r1 =
        controller.HandleControlTransfer(get_dev_desc, {}, MutableByteSpan{in_buf.data(), 18});
    EXPECT_TRUE(r1.ok());
    EXPECT_EQ(r1.bytes_transferred, 18U);
    EXPECT_EQ(in_buf[0], 18);   // bLength
    EXPECT_EQ(in_buf[1], 0x01); // bDescriptorType DEVICE
    EXPECT_EQ(in_buf[8], 0x5E); // idVendor Microsoft 0x045E (LSB)
    EXPECT_EQ(in_buf[9], 0x04); // idVendor MSB

    // 2. Get Configuration Descriptor
    UsbSetupPacket get_cfg_desc{0x80, requests::kGetDescriptor, 0x0200, 0, 32};
    auto r2 =
        controller.HandleControlTransfer(get_cfg_desc, {}, MutableByteSpan{in_buf.data(), 32});
    EXPECT_TRUE(r2.ok());
    EXPECT_EQ(r2.bytes_transferred, 32U);
    EXPECT_EQ(in_buf[1], 0x02);  // CONFIGURATION
    EXPECT_EQ(in_buf[14], 0x58); // bInterfaceClass XID (0x58)

    // 3. Get XID Descriptor (Type 0x42)
    UsbSetupPacket get_xid_desc{0x80, requests::kGetDescriptor, 0x4200, 0, 16};
    auto r3 =
        controller.HandleControlTransfer(get_xid_desc, {}, MutableByteSpan{in_buf.data(), 16});
    EXPECT_TRUE(r3.ok());
    EXPECT_EQ(r3.bytes_transferred, 16U);
    EXPECT_EQ(in_buf[1], 0x42); // XID

    // 4. Set Address
    UsbSetupPacket set_addr{0x00, requests::kSetAddress, 5, 0, 0};
    auto r4 = controller.HandleControlTransfer(set_addr, {}, {});
    EXPECT_TRUE(r4.ok());
    EXPECT_EQ(controller.address(), 5U);

    // 5. Rumble SET_REPORT (Req 0x09)
    u8 rumble_data[6] = {0x00, 0x06, 0x34, 0x12, 0x78, 0x56};
    UsbSetupPacket set_rumble{0x21, 0x09, 0x0200, 0, 6};
    auto r5 = controller.HandleControlTransfer(set_rumble, ByteSpan{rumble_data, 6}, {});
    EXPECT_TRUE(r5.ok());
    EXPECT_EQ(controller.rumble_left_motor(), 0x1234);
    EXPECT_EQ(controller.rumble_right_motor(), 0x5678);

    // 6. Unknown request must STALL
    UsbSetupPacket unk_req{0x00, 0x77, 0, 0, 0};
    auto r6 = controller.HandleControlTransfer(unk_req, {}, {});
    EXPECT_FALSE(r6.ok());
    EXPECT_EQ(r6.status, UsbTransferStatus::Stalled);
}

TEST_CASE(TestXidHotplugAndPolling) {
    XidController controller;
    EXPECT_TRUE(controller.is_connected());

    std::vector<u8> report_buf(kXidGamepadReportSize, 0);

    // Normal poll
    auto p1 = controller.HandleInterruptTransfer(
        1, MutableByteSpan{report_buf.data(), report_buf.size()});
    EXPECT_TRUE(p1.ok());
    EXPECT_EQ(p1.bytes_transferred, 20U);

    // Disconnect
    controller.Disconnect();
    EXPECT_FALSE(controller.is_connected());

    // Poll while disconnected must return DeviceDisconnected
    auto p2 = controller.HandleInterruptTransfer(
        1, MutableByteSpan{report_buf.data(), report_buf.size()});
    EXPECT_FALSE(p2.ok());
    EXPECT_EQ(p2.status, UsbTransferStatus::DeviceDisconnected);

    // Reconnect
    controller.Connect();
    EXPECT_TRUE(controller.is_connected());
    auto p3 = controller.HandleInterruptTransfer(
        1, MutableByteSpan{report_buf.data(), report_buf.size()});
    EXPECT_TRUE(p3.ok());
}

TEST_CASE(TestDeterministicExecutions) {
    // Two identical snapshot sequences must produce bit-identical reports
    auto run_sequence = [](std::vector<std::vector<u8>>& recorded_reports) {
        XidController c;
        for (u64 seq = 1; seq <= 5; ++seq) {
            HostInputSnapshot s;
            s.sequence = seq;
            s.digital_buttons = static_cast<u8>(seq & 0xFF);
            s.thumb_lx = static_cast<i16>(seq * 1000);
            s.trigger_left = static_cast<u8>(seq * 40);
            EXPECT_TRUE(c.SubmitSnapshot(s));

            std::vector<u8> buf(kXidGamepadReportSize, 0);
            auto res = c.HandleInterruptTransfer(1, MutableByteSpan{buf.data(), buf.size()});
            EXPECT_TRUE(res.ok());
            recorded_reports.push_back(buf);
        }
    };

    std::vector<std::vector<u8>> run1;
    std::vector<std::vector<u8>> run2;
    run_sequence(run1);
    run_sequence(run2);

    EXPECT_EQ(run1.size(), run2.size());
    for (size_t i = 0; i < run1.size(); ++i) {
        EXPECT_TRUE(run1[i] == run2[i]);
    }
}

TEST_CASE(TestOhciAndXidIntegrationEndToEnd) {
    OhciController hc;
    SyntheticUsbMemory mem;
    auto xid = std::make_shared<XidController>();
    xid->set_address(1);
    EXPECT_TRUE(hc.AttachDevice(0, xid).has_value());

    // Enable Operational and Periodic List, set Master Interrupt Enable
    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcControl, ohci::control::kStateUsbOperational |
                                                          ohci::control::kPeriodicListEnable)
                    .has_value());
    EXPECT_TRUE(
        hc.WriteRegister(ohci::kRegHcInterruptEnable, ohci::interrupt::kMasterInterruptEnable |
                                                          ohci::interrupt::kWritebackDoneHead)
            .has_value());

    // Inject snapshot into controller
    HostInputSnapshot snap;
    snap.sequence = 1;
    snap.button_a = 255;
    snap.thumb_rx = 12345;
    EXPECT_TRUE(xid->SubmitSnapshot(snap));

    // Prepare buffer in guest memory for interrupt report
    GuestAddr dest_buf = mem.AllocateAligned(32, 16);
    GuestAddr dummy_tail = mem.AllocateAligned(16, 16);

    // Setup TD for Interrupt IN (PID=2)
    ohci::GeneralTransferDescriptor td;
    td.flags = (2 << 19); // PID = IN
    td.cbp = dest_buf;
    td.next_td = dummy_tail;
    td.be = dest_buf + kXidGamepadReportSize - 1;
    GuestAddr td_addr = mem.WriteTransferDescriptor(td);

    // Setup ED for Endpoint 1 IN
    ohci::EndpointDescriptor ed;
    ed.flags = 1 | (1 << 7) | (2 << 11) | (32 << 16); // Addr 1, EP 1, IN, MPS 32
    ed.head_p = td_addr;
    ed.tail_p = dummy_tail;
    ed.next_ed = 0;
    GuestAddr ed_addr = mem.WriteEndpointDescriptor(ed);

    EXPECT_TRUE(hc.WriteRegister(ohci::kRegHcPeriodCurrentED, ed_addr).has_value());

    // Process frame tick
    auto frame_res = hc.ProcessFrame(mem);
    EXPECT_TRUE(frame_res.has_value());

    // Report should have been transferred to guest memory dest_buf!
    u8 report_in_guest[kXidGamepadReportSize]{0};
    EXPECT_TRUE(mem.ReadGuest(dest_buf, MutableByteSpan{report_in_guest, kXidGamepadReportSize})
                    .has_value());
    EXPECT_EQ(report_in_guest[0], 0x00); // Report ID
    EXPECT_EQ(report_in_guest[1], 0x14); // Length 20
    EXPECT_EQ(report_in_guest[4], 255);  // Button A

    // Verify DoneHead and IRQ assertion
    EXPECT_TRUE(hc.IsIrqAsserted());
    EXPECT_EQ(hc.interrupt_status() & ohci::interrupt::kWritebackDoneHead,
              ohci::interrupt::kWritebackDoneHead);
}

int main() {
    return testing::RunAllTests();
}
