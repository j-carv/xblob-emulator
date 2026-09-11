#include "xblob/kernel/debug_sink.hpp"

#include <numeric>
#include <sstream>

namespace xblob::kernel {

BufferedDebugSink::BufferedDebugSink(std::size_t max_lines, std::size_t max_bytes)
    : max_lines_(max_lines), max_bytes_(max_bytes) {}

std::string BufferedDebugSink::SanitizeString(std::string_view raw) {
    std::string sanitized;
    sanitized.reserve(raw.size());

    for (std::size_t i = 0; i < raw.size(); ++i) {
        auto c = static_cast<unsigned char>(raw[i]);
        if (c == '\n' || c == '\r' || c == '\t') {
            sanitized.push_back(static_cast<char>(c));
        } else if (c >= 0x20 && c <= 0x7E) {
            sanitized.push_back(static_cast<char>(c));
        } else if (c >= 0xC0) {
            // Check potential valid UTF-8 sequence
            std::size_t seq_len = 0;
            if ((c & 0xE0) == 0xC0)
                seq_len = 2;
            else if ((c & 0xF0) == 0xE0)
                seq_len = 3;
            else if ((c & 0xF8) == 0xF0)
                seq_len = 4;

            bool valid = (seq_len > 0 && (i + seq_len <= raw.size()));
            if (valid) {
                for (std::size_t j = 1; j < seq_len; ++j) {
                    if ((static_cast<unsigned char>(raw[i + j]) & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
            }

            if (valid) {
                for (std::size_t j = 0; j < seq_len; ++j) {
                    sanitized.push_back(raw[i + j]);
                }
                i += seq_len - 1;
            } else {
                sanitized.push_back('?');
            }
        } else {
            sanitized.push_back('?');
        }
    }
    return sanitized;
}

void BufferedDebugSink::OutputDebugString(std::string_view message) {
    std::string sanitized = SanitizeString(message);
    if (sanitized.empty())
        return;

    while (!lines_.empty() &&
           (lines_.size() >= max_lines_ || (total_bytes_ + sanitized.size() > max_bytes_))) {
        total_bytes_ -= lines_.front().size();
        lines_.pop_front();
        ++dropped_count_;
    }

    if (sanitized.size() > max_bytes_) {
        sanitized.resize(max_bytes_);
    }

    total_bytes_ += sanitized.size();
    lines_.push_back(std::move(sanitized));
}

std::string BufferedDebugSink::GetCombinedOutput() const {
    std::string out;
    out.reserve(total_bytes_);
    for (const auto& line : lines_) {
        out.append(line);
    }
    return out;
}

void BufferedDebugSink::Clear() noexcept {
    lines_.clear();
    total_bytes_ = 0;
    dropped_count_ = 0;
}

} // namespace xblob::kernel
