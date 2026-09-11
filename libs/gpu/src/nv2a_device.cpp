#include "xblob/gpu/nv2a_device.hpp"

namespace xblob::gpu {

std::shared_ptr<Nv2aDevice> Nv2aDevice::Create(pci::PciBdf bdf) {
    auto dev = std::make_shared<Nv2aDevice>(bdf);
    dev->config_header().bar(0).SetDevice(dev);
    return dev;
}

Nv2aDevice::Nv2aDevice(pci::PciBdf bdf)
    : pci::PciDevice(bdf), front_surface_(GpuSurface::Create(640, 480).value()),
      back_surface_(GpuSurface::Create(640, 480).value()) {
    InitPciHeader();
}

void Nv2aDevice::InitPciHeader() {
    config_header_.SetVendorId(pci::kPciVendorIdNvidia);
    config_header_.SetDeviceId(pci::kPciDeviceIdNv2a);
    config_header_.SetClassCodes(pci::kPciClassDisplay, pci::kPciSubclassVga, pci::kPciProgIfVga);
    config_header_.SetRevisionId(pci::kPciRevisionNv2a);
    config_header_.SetSubsystemIds(pci::kPciVendorIdNvidia, pci::kPciDeviceIdNv2a);

    // BAR 0: 16 MB MMIO register space (non-prefetchable)
    (void)config_header_.ConfigureBar(0, 0x01000000, false, false, nullptr);
    // BAR 1: 128 MB VRAM aperture (prefetchable)
    (void)config_header_.ConfigureBar(1, 0x08000000, false, true, nullptr);
}

Result<u32> Nv2aDevice::Read(u32 offset, bus::BusAccessWidth width) {
    if (offset >= 0x01000000) {
        fault_ = GpuFault::UnknownRegister;
        return Error{ErrorCode::OutOfBounds, "NV2A MMIO read exceeds BAR0 capacity", offset};
    }
    auto res = register_file_.Read(offset, width);
    if (!res.has_value()) {
        fault_ = register_file_.last_fault();
    }
    return res;
}

Result<void> Nv2aDevice::Write(u32 offset, bus::BusAccessWidth width, u32 value) {
    if (offset >= 0x01000000) {
        fault_ = GpuFault::UnknownRegister;
        return Error{ErrorCode::OutOfBounds, "NV2A MMIO write exceeds BAR0 capacity", offset};
    }
    auto res = register_file_.Write(offset, width, value);
    if (!res.has_value()) {
        fault_ = register_file_.last_fault();
    }
    return res;
}

bool Nv2aDevice::IsInterruptAsserted() const noexcept {
    const u32 video_intr = register_file_.GetValue(kRegPvideoIntr);
    const u32 video_en = register_file_.GetValue(kRegPvideoIntrEn);
    if ((video_intr & video_en) != 0) {
        return true;
    }

    const u32 graph_intr = register_file_.GetValue(kRegPgraphIntr);
    const u32 graph_en = register_file_.GetValue(kRegPgraphIntrEn);
    if ((graph_intr & graph_en) != 0) {
        return true;
    }

    const u32 fifo_pending = register_file_.GetValue(kRegPfifoPending);
    const u32 fifo_mask = register_file_.GetValue(kRegPfifoMask);
    if ((fifo_pending & fifo_mask) != 0) {
        return true;
    }

    return false;
}

void Nv2aDevice::TriggerVideoInterrupt(u32 flags) noexcept {
    const u32 current = register_file_.GetValue(kRegPvideoIntr);
    register_file_.SetValue(kRegPvideoIntr, current | flags);
}

void Nv2aDevice::AcknowledgeVideoInterrupt(u32 flags) noexcept {
    const u32 current = register_file_.GetValue(kRegPvideoIntr);
    register_file_.SetValue(kRegPvideoIntr, current & ~flags);
}

void Nv2aDevice::Flip() noexcept {
    front_surface_ = back_surface_;
    front_surface_.IncrementSequence();
    TriggerVideoInterrupt(0x00000002); // Bit 1: Flip complete
    ++frame_counter_;
}

void Nv2aDevice::Reset() noexcept {
    register_file_.Reset();
    front_surface_ = GpuSurface::Create(640, 480).value();
    back_surface_ = GpuSurface::Create(640, 480).value();
    fault_ = GpuFault::None;
    frame_counter_ = 0;
    pending_event_ids_.clear();
    InitPciHeader();
}

} // namespace xblob::gpu
