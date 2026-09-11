#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/safe_math.hpp"
#include "xblob/common/types.hpp"
#include "xblob/io/byte_source.hpp"

#include <memory>
#include <span>

namespace xblob {

class SubrangeByteSource final : public ByteSource {
public:
    [[nodiscard]] static Result<std::shared_ptr<SubrangeByteSource>>
    Create(std::shared_ptr<const ByteSource> parent, u64 base_offset, u64 size);

    ~SubrangeByteSource() override = default;

    [[nodiscard]] u64 size() const noexcept override { return size_; }
    [[nodiscard]] u64 base_offset() const noexcept { return base_offset_; }
    [[nodiscard]] const std::shared_ptr<const ByteSource>& parent() const noexcept {
        return parent_;
    }

    [[nodiscard]] Result<void> ReadAt(u64 offset, std::span<u8> dst) const override;
    [[nodiscard]] Result<ByteSpan> SpanAt(u64 offset, size_t count) const override;

private:
    SubrangeByteSource(std::shared_ptr<const ByteSource> parent, u64 base_offset,
                       u64 size) noexcept;

    std::shared_ptr<const ByteSource> parent_;
    u64 base_offset_{0};
    u64 size_{0};
};

} // namespace xblob
