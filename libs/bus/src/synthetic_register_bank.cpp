#include "xblob/bus/synthetic_register_bank.hpp"

namespace xblob::bus {

SyntheticRegisterBank::SyntheticRegisterBank(std::size_t size_bytes) : storage_(size_bytes, 0) {}

Result<u32> SyntheticRegisterBank::Read(u32 offset, BusAccessWidth width) {
    const u32 w = static_cast<u32>(width);
    if (static_cast<u64>(offset) + static_cast<u64>(w) > storage_.size()) {
        return Error{ErrorCode::OutOfBounds,
                     "Leitura em SyntheticRegisterBank além do limite suportado", offset};
    }

    u32 result = 0;
    for (u32 i = 0; i < w; ++i) {
        result |= static_cast<u32>(storage_[offset + i]) << (i * 8);
    }

    log_.push_back(SyntheticRegisterLogEntry{
        .is_write = false,
        .offset = offset,
        .width = width,
        .value = result,
    });

    return result;
}

Result<void> SyntheticRegisterBank::Write(u32 offset, BusAccessWidth width, u32 value) {
    const u32 w = static_cast<u32>(width);
    if (static_cast<u64>(offset) + static_cast<u64>(w) > storage_.size()) {
        return Error{ErrorCode::OutOfBounds,
                     "Escrita em SyntheticRegisterBank além do limite suportado", offset};
    }

    for (u32 i = 0; i < w; ++i) {
        storage_[offset + i] = static_cast<u8>((value >> (i * 8)) & 0xFF);
    }

    log_.push_back(SyntheticRegisterLogEntry{
        .is_write = true,
        .offset = offset,
        .width = width,
        .value = value,
    });

    return {};
}

} // namespace xblob::bus
