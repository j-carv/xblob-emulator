#pragma once

#include "xblob/common/result.hpp"
#include "xblob/formats/media_type.hpp"
#include "xblob/formats/xbe.hpp"
#include "xblob/formats/xiso.hpp"
#include "xblob/io/byte_source.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace xblob {

struct MediaReport {
    MediaType type{MediaType::Unknown};
    u64 file_size{0};
    std::string file_path{};
    std::optional<XbeInfo> xbe{std::nullopt};
    std::optional<XisoInfo> xiso{std::nullopt};

    [[nodiscard]] std::string FormatHumanReadable() const;
};

class MediaInspector {
public:
    [[nodiscard]] static Result<MediaReport> Inspect(const ByteSource& source,
                                                     std::string_view file_path = "");

    [[nodiscard]] static Result<MediaReport> InspectFile(const std::filesystem::path& path);
};

} // namespace xblob
