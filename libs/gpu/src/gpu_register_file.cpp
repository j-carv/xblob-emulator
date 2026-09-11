#include "xblob/gpu/gpu_register_file.hpp"

namespace xblob::gpu {

GpuRegisterFile::GpuRegisterFile() {
    InitRegisters();
}

void GpuRegisterFile::InitRegisters() {
    registers_.clear();
    last_fault_ = GpuFault::None;

    // PMC Boot 0: Chip revision / identification (Read-only)
    registers_[kRegPmcBoot0] = RegisterDesc{
        .read_mask = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000000,
        .reset_value = kNv2aChipIdRevision,
        .current_value = kNv2aChipIdRevision,
    };

    // PMC Enable: Subsystem hardware enable (Timer, Graph, Fifo)
    registers_[kRegPmcEnable] = RegisterDesc{
        .read_mask = 0x00011001,
        .write_mask = 0x00011001,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00011001,
        .current_value = 0x00011001,
    };

    // PTIMER Time 0 & 1: Hardware timer registers (Read-only)
    registers_[kRegPtimerTime0] = RegisterDesc{
        .read_mask = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPtimerTime1] = RegisterDesc{
        .read_mask = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };

    // PFIFO Interrupt status and mask
    registers_[kRegPfifoPending] = RegisterDesc{
        .read_mask = 0x00000111,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000111,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPfifoMask] = RegisterDesc{
        .read_mask = 0x00000111,
        .write_mask = 0x00000111,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };

    // PFIFO RAM addresses and mode
    registers_[kRegPfifoRamHt] = RegisterDesc{
        .read_mask = 0xFFFFFF00,
        .write_mask = 0xFFFFFF00,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPfifoRamFc] = RegisterDesc{
        .read_mask = 0xFFFFFF00,
        .write_mask = 0xFFFFFF00,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPfifoRamRo] = RegisterDesc{
        .read_mask = 0xFFFFFF00,
        .write_mask = 0xFFFFFF00,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPfifoMode] = RegisterDesc{
        .read_mask = 0x00000001,
        .write_mask = 0x00000001,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };

    // PGRAPH Interrupt and status
    registers_[kRegPgraphIntr] = RegisterDesc{
        .read_mask = 0x00010001,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00010001,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPgraphIntrEn] = RegisterDesc{
        .read_mask = 0x00010001,
        .write_mask = 0x00010001,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPgraphStatus] = RegisterDesc{
        .read_mask = 0x00000001,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };

    // PVIDEO (Display / Framebuffer) Interrupt and Buffer pointer
    registers_[kRegPvideoIntr] = RegisterDesc{
        .read_mask = 0x00000003,
        .write_mask = 0x00000000,
        .w1c_mask = 0x00000003,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPvideoIntrEn] = RegisterDesc{
        .read_mask = 0x00000003,
        .write_mask = 0x00000003,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
    registers_[kRegPvideoBuffer] = RegisterDesc{
        .read_mask = 0xFFFFFFF0,
        .write_mask = 0xFFFFFFF0,
        .w1c_mask = 0x00000000,
        .reset_value = 0x00000000,
        .current_value = 0x00000000,
    };
}

void GpuRegisterFile::Reset() noexcept {
    for (auto& [offset, desc] : registers_) {
        desc.current_value = desc.reset_value;
    }
    last_fault_ = GpuFault::None;
}

bool GpuRegisterFile::IsAllowlisted(u32 offset) const noexcept {
    return registers_.find(offset) != registers_.end();
}

Result<u32> GpuRegisterFile::Read(u32 offset, bus::BusAccessWidth width) noexcept {
    if (width != bus::BusAccessWidth::Dword) {
        last_fault_ = GpuFault::MisalignedRegisterAccess;
        return Error{ErrorCode::UnsupportedAccessSize, "Only 32-bit MMIO accesses supported",
                     offset};
    }
    if ((offset % 4) != 0) {
        last_fault_ = GpuFault::MisalignedRegisterAccess;
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit MMIO register read", offset};
    }

    auto it = registers_.find(offset);
    if (it == registers_.end()) {
        last_fault_ = GpuFault::UnknownRegister;
        return Error{ErrorCode::UnmappedAddress, "Unknown MMIO register read", offset};
    }

    return it->second.current_value & it->second.read_mask;
}

Result<void> GpuRegisterFile::Write(u32 offset, bus::BusAccessWidth width, u32 value) noexcept {
    if (width != bus::BusAccessWidth::Dword) {
        last_fault_ = GpuFault::MisalignedRegisterAccess;
        return Error{ErrorCode::UnsupportedAccessSize, "Only 32-bit MMIO accesses supported",
                     offset};
    }
    if ((offset % 4) != 0) {
        last_fault_ = GpuFault::MisalignedRegisterAccess;
        return Error{ErrorCode::InvalidArgument, "Misaligned 32-bit MMIO register write", offset};
    }

    auto it = registers_.find(offset);
    if (it == registers_.end()) {
        last_fault_ = GpuFault::UnknownRegister;
        return Error{ErrorCode::UnmappedAddress, "Unknown MMIO register write", offset};
    }

    auto& desc = it->second;

    // Apply W1C (write-1-to-clear)
    if (desc.w1c_mask != 0) {
        const u32 bits_to_clear = value & desc.w1c_mask;
        desc.current_value &= ~bits_to_clear;
    }

    // Apply writable bits
    if (desc.write_mask != 0) {
        desc.current_value = (desc.current_value & ~desc.write_mask) | (value & desc.write_mask);
    }

    return {};
}

u32 GpuRegisterFile::GetValue(u32 offset) const noexcept {
    auto it = registers_.find(offset);
    if (it != registers_.end()) {
        return it->second.current_value;
    }
    return 0;
}

void GpuRegisterFile::SetValue(u32 offset, u32 value) noexcept {
    auto it = registers_.find(offset);
    if (it != registers_.end()) {
        it->second.current_value = value;
    }
}

} // namespace xblob::gpu
