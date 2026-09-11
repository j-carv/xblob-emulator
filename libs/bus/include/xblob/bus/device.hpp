#pragma once

#include "xblob/bus/bus_types.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <string_view>

namespace xblob::bus {

class BusDevice {
public:
    virtual ~BusDevice() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual Result<u32> Read(u32 offset, BusAccessWidth width) = 0;
    [[nodiscard]] virtual Result<void> Write(u32 offset, BusAccessWidth width, u32 value) = 0;
};

} // namespace xblob::bus
