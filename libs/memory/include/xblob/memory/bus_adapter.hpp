#pragma once

#include "xblob/bus/bus.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/mmio.hpp"

namespace xblob::memory {

[[nodiscard]] MmioHandler CreateBusMmioHandler(bus::Bus& bus, GuestAddr bus_base_addr = 0);

[[nodiscard]] Result<void> MapBusMmio(AddressSpace& space, GuestAddr base, GuestSize size,
                                      bus::Bus& bus,
                                      MemoryPermission perms = MemoryPermission::ReadWrite);

} // namespace xblob::memory
