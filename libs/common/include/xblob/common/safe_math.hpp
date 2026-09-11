#pragma once

#include <concepts>
#include <cstdint>
#include <limits>

namespace xblob {

template <std::unsigned_integral T>
[[nodiscard]] constexpr bool CheckedAdd(T a, T b, T& out) noexcept {
    if (a > std::numeric_limits<T>::max() - b) {
        return false;
    }
    out = a + b;
    return true;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr bool CheckedSub(T a, T b, T& out) noexcept {
    if (b > a) {
        return false;
    }
    out = a - b;
    return true;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr bool CheckedMul(T a, T b, T& out) noexcept {
    if (a != 0 && b > std::numeric_limits<T>::max() / a) {
        return false;
    }
    out = a * b;
    return true;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr bool RangeInBounds(T offset, T size, T total_size) noexcept {
    T end = 0;
    if (!CheckedAdd(offset, size, end)) {
        return false;
    }
    return end <= total_size;
}

[[nodiscard]] bool CheckedAddU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept;
[[nodiscard]] bool CheckedSubU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept;
[[nodiscard]] bool CheckedMulU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept;
[[nodiscard]] bool RangeInBoundsU64(std::uint64_t offset, std::uint64_t size,
                                    std::uint64_t total_size) noexcept;

} // namespace xblob
