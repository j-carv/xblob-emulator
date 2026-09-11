#include "xblob/memory/bus_adapter.hpp"

namespace xblob::memory {

MmioHandler CreateBusMmioHandler(bus::Bus& bus, GuestAddr bus_base_addr) {
    auto read_fn = [&bus, bus_base_addr](u32 offset, AccessWidth width) -> Result<u32> {
        const auto bus_width = static_cast<bus::BusAccessWidth>(static_cast<u8>(width));
        const GuestAddr bus_addr = bus_base_addr + offset;
        return bus.Read(bus_addr, bus_width);
    };

    auto write_fn = [&bus, bus_base_addr](u32 offset, AccessWidth width,
                                          u32 value) -> Result<void> {
        const auto bus_width = static_cast<bus::BusAccessWidth>(static_cast<u8>(width));
        const GuestAddr bus_addr = bus_base_addr + offset;
        return bus.Write(bus_addr, bus_width, value);
    };

    return MmioHandler{
        .read = std::move(read_fn),
        .write = std::move(write_fn),
    };
}

Result<void> MapBusMmio(AddressSpace& space, GuestAddr base, GuestSize size, bus::Bus& bus,
                        MemoryPermission perms) {
    return space.MapMmio(base, size, CreateBusMmioHandler(bus, base), perms);
}

} // namespace xblob::memory
