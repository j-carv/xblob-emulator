#pragma once

#include "xblob/io/byte_source.hpp"

#include <filesystem>
#include <fstream>

namespace xblob {

class FileSource final : public ByteSource {
public:
    ~FileSource() override;

    [[nodiscard]] static Result<std::unique_ptr<FileSource>>
    Open(const std::filesystem::path& path);

    [[nodiscard]] u64 size() const noexcept override { return file_size_; }

    [[nodiscard]] Result<void> ReadAt(u64 offset, std::span<u8> dst) const override;

    // For file-backed streams, SpanAt can buffer into memory if requested or return error if not
    // preloaded.
    [[nodiscard]] Result<ByteSpan> SpanAt(u64 offset, size_t count) const override;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    FileSource(std::filesystem::path path, u64 file_size);

    std::filesystem::path path_;
    u64 file_size_{0};
    mutable std::ifstream stream_;
    mutable std::vector<u8> memory_buffer_{}; // populated lazily or on demand if span is needed
};

} // namespace xblob
