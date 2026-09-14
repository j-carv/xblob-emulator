#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/usb/ohci_registers.hpp"
#include "xblob/usb/usb_device.hpp"
#include "xblob/usb/usb_memory.hpp"

#include <array>
#include <memory>

namespace xblob::usb::ohci {
class OhciTraversalEngine;
}

namespace xblob::usb {

class OhciController {
public:
    OhciController();
    ~OhciController();

    // Reset all registers, lists, ports, and internal state
    void Reset() noexcept;

    // Register access (MMIO 32-bit)
    [[nodiscard]] Result<u32> ReadRegister(u32 offset) const noexcept;
    [[nodiscard]] Result<void> WriteRegister(u32 offset, u32 value) noexcept;

    // Root hub device management
    [[nodiscard]] Result<void> AttachDevice(u8 port_index,
                                            std::shared_ptr<UsbDevice> device) noexcept;
    [[nodiscard]] Result<void> DetachDevice(u8 port_index) noexcept;
    [[nodiscard]] std::shared_ptr<UsbDevice> GetAttachedDevice(u8 port_index) const noexcept;
    [[nodiscard]] std::shared_ptr<UsbDevice> FindDeviceByAddress(u8 address) const noexcept;

    // IRQ status
    [[nodiscard]] bool IsIrqAsserted() const noexcept;

    // Frame processing (1ms tick in hardware)
    [[nodiscard]] Result<void> ProcessFrame(UsbGuestMemory& memory) noexcept;

    // Register getters for state inspection
    [[nodiscard]] u32 control() const noexcept { return control_; }
    [[nodiscard]] u32 command_status() const noexcept { return command_status_; }
    [[nodiscard]] u32 interrupt_status() const noexcept { return interrupt_status_; }
    [[nodiscard]] u32 interrupt_enable() const noexcept { return interrupt_enable_; }
    [[nodiscard]] u32 frame_number() const noexcept { return fm_number_; }
    [[nodiscard]] u32 control_head_ed() const noexcept { return control_head_ed_; }
    [[nodiscard]] u32 bulk_head_ed() const noexcept { return bulk_head_ed_; }
    [[nodiscard]] u32 hcca() const noexcept { return hcca_; }
    [[nodiscard]] u32 port_status(u8 port_index) const noexcept;

private:
    void SetInterruptBit(u32 bit) noexcept;
    void CompletePortReset(u8 port_index) noexcept;

    // OHCI Registers
    u32 revision_{ohci::kDefaultRevision};
    u32 control_{0};
    u32 command_status_{0};
    u32 interrupt_status_{0};
    u32 interrupt_enable_{0};
    u32 hcca_{0};
    u32 period_current_ed_{0};
    u32 control_head_ed_{0};
    u32 control_current_ed_{0};
    u32 bulk_head_ed_{0};
    u32 bulk_current_ed_{0};
    u32 done_head_{0};
    u32 fm_interval_{ohci::kDefaultFmIntervalFull};
    u32 fm_remaining_{ohci::kDefaultFmInterval};
    u32 fm_number_{0};
    u32 periodic_start_{ohci::kDefaultPeriodicStart};
    u32 ls_threshold_{ohci::kDefaultLSThreshold};
    u32 rh_descriptor_a_{0x02000000 |
                         ohci::kMaxRootHubPorts}; // 4 ports, power switched individually
    u32 rh_descriptor_b_{0};
    u32 rh_status_{0};
    std::unique_ptr<ohci::OhciTraversalEngine> traversal_;

    struct PortState {
        u32 status{0};
        std::shared_ptr<UsbDevice> device{nullptr};
    };

    std::array<PortState, ohci::kMaxRootHubPorts> ports_{};
};

} // namespace xblob::usb
