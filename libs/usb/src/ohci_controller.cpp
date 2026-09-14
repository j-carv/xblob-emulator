#include "xblob/usb/ohci_controller.hpp"

#include "ohci_traversal.hpp"

namespace xblob::usb {

OhciController::OhciController() {
    Reset();
}

void OhciController::Reset() noexcept {
    revision_ = ohci::kDefaultRevision;
    control_ = ohci::control::kStateUsbReset;
    command_status_ = 0;
    interrupt_status_ = 0;
    interrupt_enable_ = 0;
    hcca_ = 0;
    period_current_ed_ = 0;
    control_head_ed_ = 0;
    control_current_ed_ = 0;
    bulk_head_ed_ = 0;
    bulk_current_ed_ = 0;
    done_head_ = 0;
    fm_interval_ = ohci::kDefaultFmIntervalFull;
    fm_remaining_ = ohci::kDefaultFmInterval;
    fm_number_ = 0;
    periodic_start_ = ohci::kDefaultPeriodicStart;
    ls_threshold_ = ohci::kDefaultLSThreshold;
    rh_descriptor_a_ = 0x02000000 | ohci::kMaxRootHubPorts;
    rh_descriptor_b_ = 0;
    rh_status_ = 0;

    for (u8 i = 0; i < ohci::kMaxRootHubPorts; ++i) {
        u32 st = ohci::port_status::kPortPowerStatus;
        if (ports_[i].device && ports_[i].device->is_connected()) {
            st |= ohci::port_status::kCurrentConnectStatus;
            st |= ohci::port_status::kConnectStatusChange;
            if (ports_[i].device->speed() == UsbSpeed::LowSpeed) {
                st |= ohci::port_status::kLowSpeedDeviceAttached;
            }
            ports_[i].device->Reset();
        }
        ports_[i].status = st;
    }
}

Result<u32> OhciController::ReadRegister(u32 offset) const noexcept {
    switch (offset) {
    case ohci::kRegHcRevision:
        return revision_;
    case ohci::kRegHcControl:
        return control_;
    case ohci::kRegHcCommandStatus:
        return command_status_;
    case ohci::kRegHcInterruptStatus:
        return interrupt_status_;
    case ohci::kRegHcInterruptEnable:
    case ohci::kRegHcInterruptDisable:
        return interrupt_enable_;
    case ohci::kRegHcHCCA:
        return hcca_;
    case ohci::kRegHcPeriodCurrentED:
        return period_current_ed_;
    case ohci::kRegHcControlHeadED:
        return control_head_ed_;
    case ohci::kRegHcControlCurrentED:
        return control_current_ed_;
    case ohci::kRegHcBulkHeadED:
        return bulk_head_ed_;
    case ohci::kRegHcBulkCurrentED:
        return bulk_current_ed_;
    case ohci::kRegHcDoneHead:
        return done_head_;
    case ohci::kRegHcFmInterval:
        return fm_interval_;
    case ohci::kRegHcFmRemaining:
        return fm_remaining_;
    case ohci::kRegHcFmNumber:
        return fm_number_;
    case ohci::kRegHcPeriodicStart:
        return periodic_start_;
    case ohci::kRegHcLSThreshold:
        return ls_threshold_;
    case ohci::kRegHcRhDescriptorA:
        return rh_descriptor_a_;
    case ohci::kRegHcRhDescriptorB:
        return rh_descriptor_b_;
    case ohci::kRegHcRhStatus:
        return rh_status_;
    default:
        if (offset >= ohci::kRegHcRhPortStatusBase &&
            offset < ohci::kRegHcRhPortStatusBase +
                         (ohci::kMaxRootHubPorts * ohci::kPortRegisterStride)) {
            u8 port = static_cast<u8>((offset - ohci::kRegHcRhPortStatusBase) /
                                      ohci::kPortRegisterStride);
            return ports_[port].status;
        }
        return Error{ErrorCode::OutOfBounds, "Offset de registro OHCI inválido", offset};
    }
}

Result<void> OhciController::WriteRegister(u32 offset, u32 value) noexcept {
    switch (offset) {
    case ohci::kRegHcRevision:
        // Read-only
        return Result<void>::Ok();
    case ohci::kRegHcControl:
        control_ = value;
        return Result<void>::Ok();
    case ohci::kRegHcCommandStatus:
        if (value & ohci::command_status::kHostControllerReset) {
            Reset();
        } else {
            command_status_ = value;
        }
        return Result<void>::Ok();
    case ohci::kRegHcInterruptStatus:
        // W1C: writing 1 clears respective bit
        interrupt_status_ &= ~value;
        return Result<void>::Ok();
    case ohci::kRegHcInterruptEnable:
        // Writing 1 sets the respective enable bit
        interrupt_enable_ |= (value & (ohci::interrupt::kValidInterruptMask |
                                       ohci::interrupt::kMasterInterruptEnable));
        return Result<void>::Ok();
    case ohci::kRegHcInterruptDisable:
        // Writing 1 clears the respective enable bit
        interrupt_enable_ &= ~(value & (ohci::interrupt::kValidInterruptMask |
                                        ohci::interrupt::kMasterInterruptEnable));
        return Result<void>::Ok();
    case ohci::kRegHcHCCA:
        hcca_ = value & ~0xFFU;
        return Result<void>::Ok();
    case ohci::kRegHcPeriodCurrentED:
        period_current_ed_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcControlHeadED:
        control_head_ed_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcControlCurrentED:
        control_current_ed_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcBulkHeadED:
        bulk_head_ed_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcBulkCurrentED:
        bulk_current_ed_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcDoneHead:
        done_head_ = value & ~0x0FU;
        return Result<void>::Ok();
    case ohci::kRegHcFmInterval:
        fm_interval_ = value;
        return Result<void>::Ok();
    case ohci::kRegHcFmRemaining:
        // Read-only in real hardware
        return Result<void>::Ok();
    case ohci::kRegHcFmNumber:
        fm_number_ = value & 0xFFFF;
        return Result<void>::Ok();
    case ohci::kRegHcPeriodicStart:
        periodic_start_ = value & 0x3FFF;
        return Result<void>::Ok();
    case ohci::kRegHcLSThreshold:
        ls_threshold_ = value & 0x0FFF;
        return Result<void>::Ok();
    case ohci::kRegHcRhDescriptorA:
    case ohci::kRegHcRhDescriptorB:
    case ohci::kRegHcRhStatus:
        return Result<void>::Ok();
    default:
        if (offset >= ohci::kRegHcRhPortStatusBase &&
            offset < ohci::kRegHcRhPortStatusBase +
                         (ohci::kMaxRootHubPorts * ohci::kPortRegisterStride)) {
            u8 port = static_cast<u8>((offset - ohci::kRegHcRhPortStatusBase) /
                                      ohci::kPortRegisterStride);

            // Handle W1C status change bits
            ports_[port].status &= ~(value & ohci::port_status::kStatusChangeMask);

            // Handle port reset write
            if (value & ohci::port_status::kPortResetStatus) {
                CompletePortReset(port);
            }

            // Handle port enable write
            if (value & ohci::port_status::kPortEnableStatus) {
                if (ports_[port].status & ohci::port_status::kCurrentConnectStatus) {
                    ports_[port].status |= ohci::port_status::kPortEnableStatus;
                }
            }

            // Handle port power write
            if (value & ohci::port_status::kPortPowerStatus) {
                ports_[port].status |= ohci::port_status::kPortPowerStatus;
            }

            return Result<void>::Ok();
        }
        return Error{ErrorCode::OutOfBounds, "Offset de registro OHCI inválido", offset};
    }
}

void OhciController::CompletePortReset(u8 port_index) noexcept {
    if (port_index >= ohci::kMaxRootHubPorts) {
        return;
    }
    auto& port = ports_[port_index];
    if (port.status & ohci::port_status::kCurrentConnectStatus) {
        if (port.device) {
            port.device->Reset();
        }
        port.status |= ohci::port_status::kPortEnableStatus;
        port.status |= ohci::port_status::kPortResetStatusChange;
        port.status &= ~ohci::port_status::kPortResetStatus;
        SetInterruptBit(ohci::interrupt::kRootHubStatusChange);
    }
}

void OhciController::SetInterruptBit(u32 bit) noexcept {
    interrupt_status_ |= (bit & ohci::interrupt::kValidInterruptMask);
}

bool OhciController::IsIrqAsserted() const noexcept {
    bool master = (interrupt_enable_ & ohci::interrupt::kMasterInterruptEnable) != 0;
    bool pending =
        (interrupt_status_ & (interrupt_enable_ & ~ohci::interrupt::kMasterInterruptEnable)) != 0;
    return master && pending;
}

u32 OhciController::port_status(u8 port_index) const noexcept {
    if (port_index < ohci::kMaxRootHubPorts) {
        return ports_[port_index].status;
    }
    return 0;
}

Result<void> OhciController::AttachDevice(u8 port_index,
                                          std::shared_ptr<UsbDevice> device) noexcept {
    if (port_index >= ohci::kMaxRootHubPorts) {
        return Error{ErrorCode::InvalidArgument, "Índice de porta USB raiz inválido", port_index};
    }
    ports_[port_index].device = std::move(device);
    ports_[port_index].status |=
        (ohci::port_status::kCurrentConnectStatus | ohci::port_status::kConnectStatusChange);
    if (ports_[port_index].device && ports_[port_index].device->speed() == UsbSpeed::LowSpeed) {
        ports_[port_index].status |= ohci::port_status::kLowSpeedDeviceAttached;
    } else {
        ports_[port_index].status &= ~ohci::port_status::kLowSpeedDeviceAttached;
    }
    SetInterruptBit(ohci::interrupt::kRootHubStatusChange);
    return Result<void>::Ok();
}

Result<void> OhciController::DetachDevice(u8 port_index) noexcept {
    if (port_index >= ohci::kMaxRootHubPorts) {
        return Error{ErrorCode::InvalidArgument, "Índice de porta USB raiz inválido", port_index};
    }
    ports_[port_index].device = nullptr;
    ports_[port_index].status &=
        ~(ohci::port_status::kCurrentConnectStatus | ohci::port_status::kPortEnableStatus);
    ports_[port_index].status |= ohci::port_status::kConnectStatusChange;
    SetInterruptBit(ohci::interrupt::kRootHubStatusChange);
    return Result<void>::Ok();
}

std::shared_ptr<UsbDevice> OhciController::GetAttachedDevice(u8 port_index) const noexcept {
    if (port_index < ohci::kMaxRootHubPorts) {
        return ports_[port_index].device;
    }
    return nullptr;
}

std::shared_ptr<UsbDevice> OhciController::FindDeviceByAddress(u8 address) const noexcept {
    for (u8 i = 0; i < ohci::kMaxRootHubPorts; ++i) {
        if (ports_[i].device && ports_[i].device->address() == address) {
            return ports_[i].device;
        }
    }
    return nullptr;
}

Result<void> OhciController::ProcessFrame(UsbGuestMemory& memory) noexcept {
    fm_number_ = (fm_number_ + 1) & 0xFFFF;
    SetInterruptBit(ohci::interrupt::kStartOfFrame);

    u32 state = control_ & ohci::control::kHostControllerFunctionalStateMask;
    if (state != ohci::control::kStateUsbOperational) {
        return Result<void>::Ok();
    }

    auto device_lookup = [this](u8 address) -> std::shared_ptr<UsbDevice> {
        return FindDeviceByAddress(address);
    };

    u32 done_head_acc = 0;
    ohci::OhciTraversalEngine traversal;

    // Process periodic list if enabled
    if (control_ & ohci::control::kPeriodicListEnable) {
        GuestAddr periodic_head = period_current_ed_;
        if (periodic_head == 0 && hcca_ != 0) {
            GuestAddr table_entry = hcca_ + (fm_number_ & 0x1F) * 4;
            auto entry_res = memory.Read32(table_entry);
            if (entry_res) {
                periodic_head = *entry_res & ~0x0FU;
            }
        }
        if (periodic_head != 0) {
            auto p_res =
                traversal.ProcessEndpointList(periodic_head, memory, done_head_acc, device_lookup);
            if (!p_res) {
                return p_res.error();
            }
        }
    }

    // Process control list if enabled
    if (control_ & ohci::control::kControlListEnable) {
        GuestAddr ctrl_head = control_current_ed_ != 0 ? control_current_ed_ : control_head_ed_;
        if (ctrl_head != 0) {
            auto c_res =
                traversal.ProcessEndpointList(ctrl_head, memory, done_head_acc, device_lookup);
            if (!c_res) {
                return c_res.error();
            }
        }
    }

    // Process bulk list if enabled
    if (control_ & ohci::control::kBulkListEnable) {
        GuestAddr bulk_head = bulk_current_ed_ != 0 ? bulk_current_ed_ : bulk_head_ed_;
        if (bulk_head != 0) {
            auto b_res =
                traversal.ProcessEndpointList(bulk_head, memory, done_head_acc, device_lookup);
            if (!b_res) {
                return b_res.error();
            }
        }
    }

    // If any TDs completed, update DoneHead and trigger interrupt
    if (done_head_acc != 0) {
        done_head_ = done_head_acc;
        if (hcca_ != 0) {
            (void)memory.Write32(hcca_ + 0x84, done_head_);
        }
        SetInterruptBit(ohci::interrupt::kWritebackDoneHead);
    }

    return Result<void>::Ok();
}

} // namespace xblob::usb
