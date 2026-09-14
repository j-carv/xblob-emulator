#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/usb/ohci_descriptors.hpp"
#include "xblob/usb/usb_device.hpp"
#include "xblob/usb/usb_memory.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace xblob::usb::ohci {

struct TraversalStats {
    u32 eds_processed{0};
    u32 tds_processed{0};
    u32 bytes_transferred{0};
    bool done_head_updated{false};
};

class OhciTraversalEngine {
public:
    OhciTraversalEngine() = default;

    void Reset() noexcept { control_sessions_.clear(); }

    // Process a list of EDs (e.g. Control List, Bulk List, or Periodic List)
    [[nodiscard]] Result<TraversalStats> ProcessEndpointList(
        GuestAddr head_ed_addr, UsbGuestMemory& memory, u32& done_head_accumulator,
        const std::function<std::shared_ptr<UsbDevice>(u8)>& device_lookup) noexcept;

private:
    struct ControlTransferSession {
        UsbSetupPacket setup{};
        std::vector<u8> in_buffer{};
        u32 in_offset{0};
        bool active{false};
    };

    std::unordered_map<u16, ControlTransferSession> control_sessions_{};

    [[nodiscard]] Result<EndpointDescriptor>
    ReadEndpointDescriptor(GuestAddr addr, UsbGuestMemory& memory) const noexcept;

    [[nodiscard]] Result<void> WriteEndpointDescriptor(GuestAddr addr, const EndpointDescriptor& ed,
                                                       UsbGuestMemory& memory) const noexcept;

    [[nodiscard]] Result<GeneralTransferDescriptor>
    ReadTransferDescriptor(GuestAddr addr, UsbGuestMemory& memory) const noexcept;

    [[nodiscard]] Result<void> WriteTransferDescriptor(GuestAddr addr,
                                                       const GeneralTransferDescriptor& td,
                                                       UsbGuestMemory& memory) const noexcept;

    [[nodiscard]] Result<bool> ProcessSingleTd(GuestAddr td_addr, GeneralTransferDescriptor& td,
                                               EndpointDescriptor& ed, GuestAddr ed_addr,
                                               UsbGuestMemory& memory, u32& done_head_accumulator,
                                               const std::shared_ptr<UsbDevice>& device) noexcept;
};

} // namespace xblob::usb::ohci
