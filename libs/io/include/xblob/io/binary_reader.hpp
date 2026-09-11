#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/io/byte_source.hpp"

#include <string>
#include <vector>

namespace xblob {

class BinaryReader {
public:
    explicit BinaryReader(const ByteSource& source) noexcept;

    [[nodiscard]] u64 position() const noexcept { return cursor_; }

    [[nodiscard]] u64 size() const noexcept { return source_.size(); }

    [[nodiscard]] u64 remaining() const noexcept {
        if (cursor_ >= source_.size()) {
            return 0;
        }
        return source_.size() - cursor_;
    }

    [[nodiscard]] bool has_remaining(u64 count) const noexcept;

    [[nodiscard]] Result<void> Seek(u64 offset);
    [[nodiscard]] Result<void> Skip(u64 count);

    [[nodiscard]] Result<u8> ReadU8();
    [[nodiscard]] Result<u16> ReadU16LE();
    [[nodiscard]] Result<u32> ReadU32LE();
    [[nodiscard]] Result<u64> ReadU64LE();

    [[nodiscard]] Result<i8> ReadI8();
    [[nodiscard]] Result<i16> ReadI16LE();
    [[nodiscard]] Result<i32> ReadI32LE();
    [[nodiscard]] Result<i64> ReadI64LE();

    [[nodiscard]] Result<void> ReadExact(std::span<u8> dst);
    [[nodiscard]] Result<std::vector<u8>> ReadBytes(size_t count);
    [[nodiscard]] Result<std::string> ReadFixedString(size_t length);

    [[nodiscard]] Result<ByteSpan> PeekSpan(size_t count) const;

private:
    const ByteSource& source_;
    u64 cursor_{0};
};

} // namespace xblob
