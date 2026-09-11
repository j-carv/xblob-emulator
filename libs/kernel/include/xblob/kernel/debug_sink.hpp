#pragma once

#include "xblob/common/types.hpp"

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xblob::kernel {

class IDebugSink {
public:
    virtual ~IDebugSink() = default;
    virtual void OutputDebugString(std::string_view message) = 0;
};

class BufferedDebugSink : public IDebugSink {
public:
    explicit BufferedDebugSink(std::size_t max_lines = 1000, std::size_t max_bytes = 64 * 1024);

    void OutputDebugString(std::string_view message) override;

    [[nodiscard]] const std::deque<std::string>& lines() const noexcept { return lines_; }
    [[nodiscard]] std::size_t total_bytes() const noexcept { return total_bytes_; }
    [[nodiscard]] u64 dropped_count() const noexcept { return dropped_count_; }

    [[nodiscard]] std::string GetCombinedOutput() const;
    void Clear() noexcept;

    static std::string SanitizeString(std::string_view raw);

private:
    std::size_t max_lines_;
    std::size_t max_bytes_;
    std::deque<std::string> lines_;
    std::size_t total_bytes_{0};
    u64 dropped_count_{0};
};

} // namespace xblob::kernel
