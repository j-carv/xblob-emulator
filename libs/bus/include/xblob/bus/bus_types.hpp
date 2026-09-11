#pragma once

#include "xblob/common/types.hpp"

#include <ostream>

namespace xblob::bus {

enum class BusAccessWidth : u8 { Byte = 1, Word = 2, Dword = 4 };

inline std::ostream& operator<<(std::ostream& os, BusAccessWidth width) {
    switch (width) {
    case BusAccessWidth::Byte:
        return os << "Byte";
    case BusAccessWidth::Word:
        return os << "Word";
    case BusAccessWidth::Dword:
        return os << "Dword";
    }
    return os << "Unknown";
}

} // namespace xblob::bus
