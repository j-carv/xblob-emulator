#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace xblob {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using ByteSpan = std::span<const u8>;
using MutableByteSpan = std::span<u8>;

using GuestAddr = u32;
using GuestSize = u32;
using Cycle = u64;
using EventId = u64;

} // namespace xblob
