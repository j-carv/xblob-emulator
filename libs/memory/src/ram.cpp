#include "xblob/memory/ram.hpp"

#include "xblob/common/safe_math.hpp"

#include <algorithm>

namespace xblob::memory {

Ram::Ram(std::size_t size_bytes) : data_(size_bytes, 0) {}

Result<Ram> Ram::Create(std::size_t size_bytes) {
    if (size_bytes != kRamSizeRetail && size_bytes != kRamSizeDevkit) {
        return Error{ErrorCode::InvalidCapacity,
                     "Capacidade de RAM física deve ser 64 MiB ou 128 MiB", size_bytes};
    }
    return Ram(size_bytes);
}

Result<u8> Ram::Read8(std::size_t offset) const noexcept {
    if (offset >= data_.size()) {
        return Error{ErrorCode::OutOfBounds, "Offset além do limite da RAM", offset};
    }
    return data_[offset];
}

Result<void> Ram::Write8(std::size_t offset, u8 value) noexcept {
    if (offset >= data_.size()) {
        return Error{ErrorCode::OutOfBounds, "Offset além do limite da RAM", offset};
    }
    data_[offset] = value;
    return Result<void>::Ok();
}

Result<ByteSpan> Ram::ReadBytes(std::size_t offset, std::size_t count) const noexcept {
    std::size_t end = 0;
    if (!CheckedAdd(offset, count, end) || end > data_.size()) {
        return Error{ErrorCode::OutOfBounds, "Faixa de leitura excede o limite da RAM", offset};
    }
    return ByteSpan{data_.data() + offset, count};
}

Result<void> Ram::WriteBytes(std::size_t offset, ByteSpan data) noexcept {
    std::size_t end = 0;
    if (!CheckedAdd(offset, data.size(), end) || end > data_.size()) {
        return Error{ErrorCode::OutOfBounds, "Faixa de escrita excede o limite da RAM", offset};
    }
    std::copy(data.begin(), data.end(), data_.begin() + static_cast<std::ptrdiff_t>(offset));
    return Result<void>::Ok();
}

} // namespace xblob::memory
