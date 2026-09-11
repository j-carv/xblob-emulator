#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/safe_math.hpp"
#include "xblob/common/types.hpp"

#include <memory>
#include <span>
#include <vector>

namespace xblob {

class ByteSource {
public:
    virtual ~ByteSource() = default;

    [[nodiscard]] virtual u64 size() const noexcept = 0;
    [[nodiscard]] virtual Result<void> ReadAt(u64 offset, std::span<u8> dst) const = 0;
    [[nodiscard]] virtual Result<ByteSpan> SpanAt(u64 offset, size_t count) const = 0;
};

class MemoryByteSource final : public ByteSource {
public:
    explicit MemoryByteSource(ByteSpan data) noexcept : data_(data) {}
    explicit MemoryByteSource(const std::vector<u8>& data) noexcept
        : data_(data.data(), data.size()) {}

    [[nodiscard]] u64 size() const noexcept override { return static_cast<u64>(data_.size()); }

    [[nodiscard]] Result<void> ReadAt(u64 offset, std::span<u8> dst) const override {
        if (!RangeInBoundsU64(offset, dst.size(), size())) {
            return Error{ErrorCode::OutOfBounds, "Leitura fora dos limites do buffer", offset};
        }
        const auto* src = data_.data() + offset;
        std::copy(src, src + dst.size(), dst.data());
        return Result<void>::Ok();
    }

    [[nodiscard]] Result<ByteSpan> SpanAt(u64 offset, size_t count) const override {
        if (!RangeInBoundsU64(offset, count, size())) {
            return Error{ErrorCode::OutOfBounds, "Span fora dos limites do buffer", offset};
        }
        return ByteSpan(data_.data() + offset, count);
    }

private:
    ByteSpan data_;
};

} // namespace xblob
