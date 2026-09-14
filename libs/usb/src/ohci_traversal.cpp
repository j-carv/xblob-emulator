#include "ohci_traversal.hpp"

#include <unordered_map>
#include <vector>

namespace xblob::usb::ohci {

Result<EndpointDescriptor>
OhciTraversalEngine::ReadEndpointDescriptor(GuestAddr addr, UsbGuestMemory& memory) const noexcept {
    if ((addr & (kDescriptorAlignment - 1)) != 0) {
        return Error{ErrorCode::OutOfBounds, "Endereço de ED desalinhado", addr};
    }
    if (!memory.IsValidRange(addr, sizeof(EndpointDescriptor))) {
        return Error{ErrorCode::AccessViolation, "Endereço de ED fora dos limites", addr};
    }

    EndpointDescriptor ed;
    auto r0 = memory.Read32(addr);
    auto r1 = memory.Read32(addr + 4);
    auto r2 = memory.Read32(addr + 8);
    auto r3 = memory.Read32(addr + 12);
    if (!r0 || !r1 || !r2 || !r3) {
        return Error{ErrorCode::AccessViolation, "Falha ao ler ED da memória guest", addr};
    }
    ed.flags = *r0;
    ed.tail_p = *r1;
    ed.head_p = *r2;
    ed.next_ed = *r3;
    return ed;
}

Result<void> OhciTraversalEngine::WriteEndpointDescriptor(GuestAddr addr,
                                                          const EndpointDescriptor& ed,
                                                          UsbGuestMemory& memory) const noexcept {
    if ((addr & (kDescriptorAlignment - 1)) != 0) {
        return Error{ErrorCode::OutOfBounds, "Endereço de ED desalinhado", addr};
    }
    if (!memory.IsValidRange(addr, sizeof(EndpointDescriptor))) {
        return Error{ErrorCode::AccessViolation, "Endereço de ED fora dos limites", addr};
    }

    auto w0 = memory.Write32(addr, ed.flags);
    auto w1 = memory.Write32(addr + 4, ed.tail_p);
    auto w2 = memory.Write32(addr + 8, ed.head_p);
    auto w3 = memory.Write32(addr + 12, ed.next_ed);
    if (!w0 || !w1 || !w2 || !w3) {
        return Error{ErrorCode::AccessViolation, "Falha ao gravar ED na memória guest", addr};
    }
    return Result<void>::Ok();
}

Result<GeneralTransferDescriptor>
OhciTraversalEngine::ReadTransferDescriptor(GuestAddr addr, UsbGuestMemory& memory) const noexcept {
    if ((addr & (kDescriptorAlignment - 1)) != 0) {
        return Error{ErrorCode::OutOfBounds, "Endereço de TD desalinhado", addr};
    }
    if (!memory.IsValidRange(addr, sizeof(GeneralTransferDescriptor))) {
        return Error{ErrorCode::AccessViolation, "Endereço de TD fora dos limites", addr};
    }

    GeneralTransferDescriptor td;
    auto r0 = memory.Read32(addr);
    auto r1 = memory.Read32(addr + 4);
    auto r2 = memory.Read32(addr + 8);
    auto r3 = memory.Read32(addr + 12);
    if (!r0 || !r1 || !r2 || !r3) {
        return Error{ErrorCode::AccessViolation, "Falha ao ler TD da memória guest", addr};
    }
    td.flags = *r0;
    td.cbp = *r1;
    td.next_td = *r2;
    td.be = *r3;
    return td;
}

Result<void> OhciTraversalEngine::WriteTransferDescriptor(GuestAddr addr,
                                                          const GeneralTransferDescriptor& td,
                                                          UsbGuestMemory& memory) const noexcept {
    if ((addr & (kDescriptorAlignment - 1)) != 0) {
        return Error{ErrorCode::OutOfBounds, "Endereço de TD desalinhado", addr};
    }
    if (!memory.IsValidRange(addr, sizeof(GeneralTransferDescriptor))) {
        return Error{ErrorCode::AccessViolation, "Endereço de TD fora dos limites", addr};
    }

    auto w0 = memory.Write32(addr, td.flags);
    auto w1 = memory.Write32(addr + 4, td.cbp);
    auto w2 = memory.Write32(addr + 8, td.next_td);
    auto w3 = memory.Write32(addr + 12, td.be);
    if (!w0 || !w1 || !w2 || !w3) {
        return Error{ErrorCode::AccessViolation, "Falha ao gravar TD na memória guest", addr};
    }
    return Result<void>::Ok();
}

Result<bool>
OhciTraversalEngine::ProcessSingleTd(GuestAddr td_addr, GeneralTransferDescriptor& td,
                                     EndpointDescriptor& ed, GuestAddr ed_addr,
                                     UsbGuestMemory& memory, u32& done_head_accumulator,
                                     const std::shared_ptr<UsbDevice>& device) noexcept {

    if (!device || !device->is_connected()) {
        td.set_condition_code(static_cast<u8>(UsbTransferStatus::DeviceNotResponding));
        ed.set_halted(true);
        (void)WriteTransferDescriptor(td_addr, td, memory);
        (void)WriteEndpointDescriptor(ed_addr, ed, memory);
        return false;
    }

    u8 dir = ed.direction();
    UsbPid pid = td.pid_direction();
    if (dir == 1) {
        pid = UsbPid::Out;
    } else if (dir == 2) {
        pid = UsbPid::In;
    }

    u16 session_key =
        static_cast<u16>((ed.function_address() << 4) | (ed.endpoint_number() & 0x0F));
    auto& session = control_sessions_[session_key];

    u32 buf_len = td.buffer_length();
    bool advance_and_retire = true;

    if (pid == UsbPid::Setup) {
        if (buf_len < 8 || td.cbp == 0) {
            td.set_condition_code(static_cast<u8>(UsbTransferStatus::InvalidDescriptor));
            ed.set_halted(true);
            advance_and_retire = false;
        } else {
            u8 raw_setup[8]{0};
            auto read_res = memory.ReadGuest(td.cbp, MutableByteSpan{raw_setup, 8});
            if (!read_res) {
                td.set_condition_code(static_cast<u8>(UsbTransferStatus::MemoryFault));
                ed.set_halted(true);
                advance_and_retire = false;
            } else {
                auto parsed = UsbSetupPacket::FromBytes(ByteSpan{raw_setup, 8});
                if (!parsed) {
                    td.set_condition_code(static_cast<u8>(UsbTransferStatus::InvalidDescriptor));
                    ed.set_halted(true);
                    advance_and_retire = false;
                } else {
                    session.setup = *parsed;
                    session.active = true;
                    session.in_offset = 0;
                    session.in_buffer.clear();

                    if (session.setup.is_device_to_host() && session.setup.length > 0) {
                        session.in_buffer.resize(session.setup.length);
                        auto xfer = device->HandleControlTransfer(
                            session.setup, ByteSpan{},
                            MutableByteSpan{session.in_buffer.data(), session.in_buffer.size()});
                        if (!xfer.ok()) {
                            td.set_condition_code(static_cast<u8>(xfer.status));
                            ed.set_halted(true);
                            session.active = false;
                            advance_and_retire = false;
                        } else {
                            session.in_buffer.resize(xfer.bytes_transferred);
                            td.set_condition_code(0);
                            td.cbp = 0;
                        }
                    } else {
                        td.set_condition_code(0);
                        td.cbp = 0;
                    }
                }
            }
        }
    } else if (pid == UsbPid::In) {
        if (ed.endpoint_number() == 0) {
            // Control Data IN or Status IN
            if (!session.active) {
                td.set_condition_code(static_cast<u8>(UsbTransferStatus::Stalled));
                ed.set_halted(true);
                advance_and_retire = false;
            } else if (buf_len == 0 || td.cbp == 0) {
                // Status phase IN
                td.set_condition_code(0);
                session.active = false;
            } else {
                u32 available = static_cast<u32>(session.in_buffer.size() - session.in_offset);
                u32 to_copy = (buf_len < available) ? buf_len : available;
                if (to_copy > 0) {
                    auto wres = memory.WriteGuest(
                        td.cbp, ByteSpan{session.in_buffer.data() + session.in_offset, to_copy});
                    if (!wres) {
                        td.set_condition_code(static_cast<u8>(UsbTransferStatus::MemoryFault));
                        ed.set_halted(true);
                        advance_and_retire = false;
                    } else {
                        session.in_offset += to_copy;
                        td.cbp = 0;
                        td.set_condition_code(0);
                    }
                } else {
                    td.cbp = 0;
                    td.set_condition_code(0);
                }
            }
        } else {
            // Interrupt or Bulk IN transfer
            std::vector<u8> read_buf(buf_len > 0 ? buf_len : 64);
            auto xfer = device->HandleInterruptTransfer(
                ed.endpoint_number(), MutableByteSpan{read_buf.data(), read_buf.size()});

            if (!xfer.ok()) {
                td.set_condition_code(static_cast<u8>(xfer.status));
                ed.set_halted(true);
                advance_and_retire = false;
            } else {
                u32 to_write =
                    (xfer.bytes_transferred < buf_len) ? xfer.bytes_transferred : buf_len;
                if (to_write > 0 && td.cbp != 0) {
                    auto wres = memory.WriteGuest(td.cbp, ByteSpan{read_buf.data(), to_write});
                    if (!wres) {
                        td.set_condition_code(static_cast<u8>(UsbTransferStatus::MemoryFault));
                        ed.set_halted(true);
                        advance_and_retire = false;
                    }
                }
                if (advance_and_retire) {
                    td.cbp = 0;
                    td.set_condition_code(0);
                }
            }
        }
    } else if (pid == UsbPid::Out) {
        if (ed.endpoint_number() == 0) {
            // Control Data OUT or Status OUT
            if (!session.active && buf_len > 0) {
                td.set_condition_code(static_cast<u8>(UsbTransferStatus::Stalled));
                ed.set_halted(true);
                advance_and_retire = false;
            } else if (buf_len == 0 || td.cbp == 0) {
                // Status phase OUT
                td.set_condition_code(0);
                session.active = false;
            } else {
                std::vector<u8> payload(buf_len);
                auto rres =
                    memory.ReadGuest(td.cbp, MutableByteSpan{payload.data(), payload.size()});
                if (!rres) {
                    td.set_condition_code(static_cast<u8>(UsbTransferStatus::MemoryFault));
                    ed.set_halted(true);
                    advance_and_retire = false;
                } else {
                    auto xfer = device->HandleControlTransfer(
                        session.setup, ByteSpan{payload.data(), payload.size()}, MutableByteSpan{});
                    if (!xfer.ok()) {
                        td.set_condition_code(static_cast<u8>(xfer.status));
                        ed.set_halted(true);
                        advance_and_retire = false;
                    } else {
                        td.cbp = 0;
                        td.set_condition_code(0);
                    }
                }
            }
        } else {
            // Non-control OUT: not currently used by minimal XID gamepad, but acknowledge
            td.cbp = 0;
            td.set_condition_code(0);
        }
    }

    // Update ED and TD state in memory
    u32 old_next_td = td.next_td_pointer();
    if (advance_and_retire) {
        ed.set_head_pointer(old_next_td);
        // Link into Done Queue if DelayInterrupt != 7
        if (td.delay_interrupt() != 7) {
            td.next_td = done_head_accumulator;
            done_head_accumulator = td_addr;
        }
    }

    (void)WriteTransferDescriptor(td_addr, td, memory);
    (void)WriteEndpointDescriptor(ed_addr, ed, memory);

    return advance_and_retire;
}

Result<TraversalStats> OhciTraversalEngine::ProcessEndpointList(
    GuestAddr head_ed_addr, UsbGuestMemory& memory, u32& done_head_accumulator,
    const std::function<std::shared_ptr<UsbDevice>(u8)>& device_lookup) noexcept {

    TraversalStats stats;
    GuestAddr current_ed_addr = head_ed_addr & ~0x0FU;
    std::vector<GuestAddr> visited_eds;

    while (current_ed_addr != 0) {
        if (stats.eds_processed >= kMaxEdTraversalBudget) {
            return Error{ErrorCode::LimitReached, "Orçamento de EDs excedido (possível loop)",
                         current_ed_addr};
        }

        // Cycle check
        for (GuestAddr addr : visited_eds) {
            if (addr == current_ed_addr) {
                return Error{ErrorCode::LimitReached, "Ciclo detectado na lista de EDs",
                             current_ed_addr};
            }
        }
        visited_eds.push_back(current_ed_addr);
        stats.eds_processed++;

        auto ed_res = ReadEndpointDescriptor(current_ed_addr, memory);
        if (!ed_res) {
            return ed_res.error();
        }
        auto ed = *ed_res;

        if (ed.skip() || ed.is_halted() || ed.is_queue_empty()) {
            current_ed_addr = ed.next_ed_pointer();
            continue;
        }

        auto dev = device_lookup(ed.function_address());

        // Process TDs in this ED
        GuestAddr current_td_addr = ed.head_pointer();
        std::vector<GuestAddr> visited_tds;

        while (current_td_addr != 0 && current_td_addr != ed.tail_pointer()) {
            if (stats.tds_processed >= kMaxTdTraversalBudget) {
                return Error{ErrorCode::LimitReached, "Orçamento de TDs excedido (possível loop)",
                             current_td_addr};
            }

            for (GuestAddr addr : visited_tds) {
                if (addr == current_td_addr) {
                    return Error{ErrorCode::LimitReached, "Ciclo detectado na lista de TDs",
                                 current_td_addr};
                }
            }
            visited_tds.push_back(current_td_addr);
            stats.tds_processed++;

            auto td_res = ReadTransferDescriptor(current_td_addr, memory);
            if (!td_res) {
                ed.set_halted(true);
                (void)WriteEndpointDescriptor(current_ed_addr, ed, memory);
                break;
            }
            auto td = *td_res;

            auto step_res = ProcessSingleTd(current_td_addr, td, ed, current_ed_addr, memory,
                                            done_head_accumulator, dev);

            if (!step_res || !*step_res) {
                // TD halted or error
                break;
            }

            current_td_addr = ed.head_pointer();
        }

        current_ed_addr = ed.next_ed_pointer();
    }

    return stats;
}

} // namespace xblob::usb::ohci
