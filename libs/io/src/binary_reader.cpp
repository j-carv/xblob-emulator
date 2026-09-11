#include "xblob/io/binary_reader.hpp"

#include "xblob/common/safe_math.hpp"

#include <array>

namespace xblob {

BinaryReader::BinaryReader(const ByteSource& source) noexcept : source_(source) {}

bool BinaryReader::has_remaining(u64 count) const noexcept {
    u64 end = 0;
    if (!CheckedAddU64(cursor_, count, end)) {
        return false;
    }
    return end <= source_.size();
}

Result<void> BinaryReader::Seek(u64 offset) {
    if (offset > source_.size()) {
        return Error{ErrorCode::OutOfBounds, "Offset além do tamanho da fonte de bytes", offset};
    }
    cursor_ = offset;
    return Result<void>::Ok();
}

Result<void> BinaryReader::Skip(u64 count) {
    u64 new_pos = 0;
    if (!CheckedAddU64(cursor_, count, new_pos) || new_pos > source_.size()) {
        return Error{ErrorCode::OutOfBounds, "Avanço além do tamanho da fonte de bytes", cursor_};
    }
    cursor_ = new_pos;
    return Result<void>::Ok();
}

Result<void> BinaryReader::ReadExact(std::span<u8> dst) {
    if (dst.empty()) {
        return Result<void>::Ok();
    }

    if (!has_remaining(dst.size())) {
        return Error{ErrorCode::UnexpectedEof,
                     "Fim prematuro dos dados durante leitura de bloco binário", cursor_};
    }

    auto res = source_.ReadAt(cursor_, dst);
    if (!res) {
        return res.error();
    }

    cursor_ += dst.size();
    return Result<void>::Ok();
}

Result<u8> BinaryReader::ReadU8() {
    u8 val = 0;
    auto res = ReadExact(std::span<u8>(&val, 1));
    if (!res) {
        return res.error();
    }
    return val;
}

Result<u16> BinaryReader::ReadU16LE() {
    std::array<u8, 2> buf{};
    auto res = ReadExact(std::span<u8>(buf.data(), buf.size()));
    if (!res) {
        return res.error();
    }
    const u16 val = static_cast<u16>(static_cast<u32>(buf[0]) | (static_cast<u32>(buf[1]) << 8));
    return val;
}

Result<u32> BinaryReader::ReadU32LE() {
    std::array<u8, 4> buf{};
    auto res = ReadExact(std::span<u8>(buf.data(), buf.size()));
    if (!res) {
        return res.error();
    }
    const u32 val = static_cast<u32>(buf[0]) | (static_cast<u32>(buf[1]) << 8) |
                    (static_cast<u32>(buf[2]) << 16) | (static_cast<u32>(buf[3]) << 24);
    return val;
}

Result<u64> BinaryReader::ReadU64LE() {
    std::array<u8, 8> buf{};
    auto res = ReadExact(std::span<u8>(buf.data(), buf.size()));
    if (!res) {
        return res.error();
    }
    u64 val = 0;
    for (size_t i = 0; i < 8; ++i) {
        val |= (static_cast<u64>(buf[i]) << (i * 8));
    }
    return val;
}

Result<i8> BinaryReader::ReadI8() {
    auto res = ReadU8();
    if (!res) {
        return res.error();
    }
    return static_cast<i8>(*res);
}

Result<i16> BinaryReader::ReadI16LE() {
    auto res = ReadU16LE();
    if (!res) {
        return res.error();
    }
    return static_cast<i16>(*res);
}

Result<i32> BinaryReader::ReadI32LE() {
    auto res = ReadU32LE();
    if (!res) {
        return res.error();
    }
    return static_cast<i32>(*res);
}

Result<i64> BinaryReader::ReadI64LE() {
    auto res = ReadU64LE();
    if (!res) {
        return res.error();
    }
    return static_cast<i64>(*res);
}

Result<std::vector<u8>> BinaryReader::ReadBytes(size_t count) {
    std::vector<u8> buffer(count);
    auto res = ReadExact(std::span<u8>(buffer));
    if (!res) {
        return res.error();
    }
    return buffer;
}

Result<std::string> BinaryReader::ReadFixedString(size_t length) {
    std::vector<u8> buffer(length);
    auto res = ReadExact(std::span<u8>(buffer));
    if (!res) {
        return res.error();
    }
    return std::string(reinterpret_cast<const char*>(buffer.data()), length);
}

Result<ByteSpan> BinaryReader::PeekSpan(size_t count) const {
    if (!has_remaining(count)) {
        return Error{ErrorCode::OutOfBounds, "Visualização de span além dos limites disponíveis",
                     cursor_};
    }
    return source_.SpanAt(cursor_, count);
}

} // namespace xblob
