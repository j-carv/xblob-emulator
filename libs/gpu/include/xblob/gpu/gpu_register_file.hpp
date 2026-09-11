#pragma once

#include "xblob/bus/bus_types.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/gpu_types.hpp"

#include <unordered_map>

namespace xblob::gpu {

struct RegisterDesc {
    u32 read_mask{0xFFFFFFFF};
    u32 write_mask{0x00000000};
    u32 w1c_mask{0x00000000};
    u32 reset_value{0x00000000};
    u32 current_value{0x00000000};
};

class GpuRegisterFile {
public:
    GpuRegisterFile();

    void Reset() noexcept;

    [[nodiscard]] Result<u32> Read(u32 offset, bus::BusAccessWidth width) noexcept;
    [[nodiscard]] Result<void> Write(u32 offset, bus::BusAccessWidth width, u32 value) noexcept;

    [[nodiscard]] u32 GetValue(u32 offset) const noexcept;
    void SetValue(u32 offset, u32 value) noexcept;

    [[nodiscard]] GpuFault last_fault() const noexcept { return last_fault_; }
    void ClearFault() noexcept { last_fault_ = GpuFault::None; }

    [[nodiscard]] bool IsAllowlisted(u32 offset) const noexcept;

private:
    void InitRegisters();

    std::unordered_map<u32, RegisterDesc> registers_;
    GpuFault last_fault_{GpuFault::None};
};

} // namespace xblob::gpu
