#include "xblob/common/safe_math.hpp"

namespace xblob {

bool CheckedAddU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    return CheckedAdd(a, b, out);
}

bool CheckedSubU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    return CheckedSub(a, b, out);
}

bool CheckedMulU64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    return CheckedMul(a, b, out);
}

bool RangeInBoundsU64(std::uint64_t offset, std::uint64_t size, std::uint64_t total_size) noexcept {
    return RangeInBounds(offset, size, total_size);
}

} // namespace xblob
