#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <cstddef>
#include <vector>

namespace xblob::memory {

inline constexpr std::size_t kRamSizeRetail = 64 * 1024 * 1024;  // 64 MiB
inline constexpr std::size_t kRamSizeDevkit = 128 * 1024 * 1024; // 128 MiB

class Ram {
public:
    static Result<Ram> Create(std::size_t size_bytes);

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] ByteSpan span() const noexcept { return ByteSpan{data_.data(), data_.size()}; }
    [[nodiscard]] MutableByteSpan mutable_span() noexcept {
        return MutableByteSpan{data_.data(), data_.size()};
    }

    [[nodiscard]] Result<u8> Read8(std::size_t offset) const noexcept;
    [[nodiscard]] Result<void> Write8(std::size_t offset, u8 value) noexcept;

    [[nodiscard]] Result<ByteSpan> ReadBytes(std::size_t offset, std::size_t count) const noexcept;
    [[nodiscard]] Result<void> WriteBytes(std::size_t offset, ByteSpan data) noexcept;

    // Direct access helper for internal mapping within bounds
    [[nodiscard]] const u8* data() const noexcept { return data_.data(); }
    [[nodiscard]] u8* data() noexcept { return data_.data(); }

private:
    explicit Ram(std::size_t size_bytes);

    std::vector<u8> data_;
};

} // namespace xblob::memory
