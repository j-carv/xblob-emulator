#include "xblob/io/subrange_byte_source.hpp"

namespace xblob {

SubrangeByteSource::SubrangeByteSource(std::shared_ptr<const ByteSource> parent, u64 base_offset,
                                       u64 size) noexcept
    : parent_(std::move(parent)), base_offset_(base_offset), size_(size) {}

Result<std::shared_ptr<SubrangeByteSource>>
SubrangeByteSource::Create(std::shared_ptr<const ByteSource> parent, u64 base_offset, u64 size) {
    if (!parent) {
        return Error{ErrorCode::InvalidArgument, "Parent ByteSource cannot be null"};
    }

    if (!RangeInBoundsU64(base_offset, size, parent->size())) {
        return Error{ErrorCode::OutOfBounds, "Subrange exceeds parent ByteSource bounds",
                     base_offset};
    }

    return std::shared_ptr<SubrangeByteSource>(
        new SubrangeByteSource(std::move(parent), base_offset, size));
}

Result<void> SubrangeByteSource::ReadAt(u64 offset, std::span<u8> dst) const {
    if (dst.empty()) {
        return Result<void>::Ok();
    }

    if (!RangeInBoundsU64(offset, dst.size(), size_)) {
        return Error{ErrorCode::OutOfBounds, "Read out of subrange bounds", offset};
    }

    u64 parent_offset = 0;
    if (!CheckedAddU64(base_offset_, offset, parent_offset)) {
        return Error{ErrorCode::IntegerOverflow, "Subrange offset overflowed"};
    }

    return parent_->ReadAt(parent_offset, dst);
}

Result<ByteSpan> SubrangeByteSource::SpanAt(u64 offset, size_t count) const {
    if (!RangeInBoundsU64(offset, count, size_)) {
        return Error{ErrorCode::OutOfBounds, "Span out of subrange bounds", offset};
    }

    u64 parent_offset = 0;
    if (!CheckedAddU64(base_offset_, offset, parent_offset)) {
        return Error{ErrorCode::IntegerOverflow, "Subrange offset overflowed"};
    }

    return parent_->SpanAt(parent_offset, count);
}

} // namespace xblob
