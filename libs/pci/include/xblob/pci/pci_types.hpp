#pragma once

#include "xblob/common/types.hpp"

#include <compare>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace xblob::pci {

enum class PciAccessWidth : u8 {
    Byte = 1,
    Word = 2,
    Dword = 4,
};

struct PciBdf {
    u8 bus{0};
    u8 device{0};
    u8 function{0};

    [[nodiscard]] constexpr u16 raw() const noexcept {
        return static_cast<u16>((static_cast<u16>(bus) << 8) |
                                (static_cast<u16>(device & 0x1F) << 3) |
                                (static_cast<u16>(function & 0x07)));
    }

    [[nodiscard]] constexpr bool operator==(const PciBdf& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const PciBdf& other) const noexcept = default;

    [[nodiscard]] std::string ToString() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bus) << ":"
            << std::setw(2) << static_cast<int>(device) << "." << static_cast<int>(function);
        return oss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const PciBdf& bdf) {
    return os << bdf.ToString();
}

// PCI Configuration Header Type 0 Register Offsets
constexpr u8 kPciVendorIdOffset = 0x00;
constexpr u8 kPciDeviceIdOffset = 0x02;
constexpr u8 kPciCommandOffset = 0x04;
constexpr u8 kPciStatusOffset = 0x06;
constexpr u8 kPciRevisionIdOffset = 0x08;
constexpr u8 kPciProgIfOffset = 0x09;
constexpr u8 kPciSubclassOffset = 0x0A;
constexpr u8 kPciBaseClassOffset = 0x0B;
constexpr u8 kPciCacheLineSizeOffset = 0x0C;
constexpr u8 kPciLatencyTimerOffset = 0x0D;
constexpr u8 kPciHeaderTypeOffset = 0x0E;
constexpr u8 kPciBistOffset = 0x0F;
constexpr u8 kPciBar0Offset = 0x10;
constexpr u8 kPciBar1Offset = 0x14;
constexpr u8 kPciBar2Offset = 0x18;
constexpr u8 kPciBar3Offset = 0x1C;
constexpr u8 kPciBar4Offset = 0x20;
constexpr u8 kPciBar5Offset = 0x24;
constexpr u8 kPciCardbusCisOffset = 0x28;
constexpr u8 kPciSubsystemVendorIdOffset = 0x2C;
constexpr u8 kPciSubsystemIdOffset = 0x2E;
constexpr u8 kPciExpansionRomOffset = 0x30;
constexpr u8 kPciCapabilitiesPtrOffset = 0x34;
constexpr u8 kPciInterruptLineOffset = 0x3C;
constexpr u8 kPciInterruptPinOffset = 0x3D;
constexpr u8 kPciMinGntOffset = 0x3E;
constexpr u8 kPciMaxLatOffset = 0x3F;

constexpr std::size_t kPciConfigSpaceSize = 256;
constexpr std::size_t kPciBarCount = 6;

// Command Register Flags
constexpr u16 kPciCommandIoSpace = 0x0001;
constexpr u16 kPciCommandMemorySpace = 0x0002;
constexpr u16 kPciCommandBusMaster = 0x0004;

// Status Register Flags
constexpr u16 kPciStatusCapabilitiesList = 0x0010;
constexpr u16 kPciStatus66MHz = 0x0020;
constexpr u16 kPciStatusFastBackToBack = 0x0080;
constexpr u16 kPciStatusDevselMedium = 0x0200;

// BAR Flags and Masks
constexpr u32 kPciBarMemoryFlag = 0x00000000;
constexpr u32 kPciBarIoFlag = 0x00000001;
constexpr u32 kPciBarMemoryType32 = 0x00000000;
constexpr u32 kPciBarPrefetchable = 0x00000008;
constexpr u32 kPciBarMemoryAddressMask = 0xFFFFFFF0;
constexpr u32 kPciBarIoAddressMask = 0xFFFFFFFC;

// Documented Clean-Room NV2A Device Identification
constexpr u16 kPciVendorIdNvidia = 0x10DE;
constexpr u16 kPciDeviceIdNv2a = 0x02A0;
constexpr u8 kPciClassDisplay = 0x03;
constexpr u8 kPciSubclassVga = 0x00;
constexpr u8 kPciProgIfVga = 0x00;
constexpr u8 kPciRevisionNv2a = 0xA1;

} // namespace xblob::pci
