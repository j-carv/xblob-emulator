#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/formats/media_type.hpp"
#include "xblob/io/byte_source.hpp"

#include <string>
#include <string_view>

namespace xblob {

constexpr std::string_view kXdvdfsMagic = "MICROSOFT*XBOX*MEDIA";
constexpr u64 kSectorSize = 2048;
constexpr u64 kXisoTrimmedDescriptorOffset = 32 * kSectorSize;     // 0x10000 = 65,536
constexpr u64 kXisoRawDescriptorOffset = 0x30620ULL * kSectorSize; // 405,864,448 bytes

struct XisoInfo {
    MediaType variant{MediaType::Unknown};
    u64 volume_descriptor_offset{0};
    u32 root_dir_sector{0};
    u32 root_dir_size{0};
    u64 creation_timestamp{0};
    bool valid_footer_magic{false};
};

class XisoDetector {
public:
    [[nodiscard]] static Result<MediaType> Detect(const ByteSource& source);
    [[nodiscard]] static Result<XisoInfo> Inspect(const ByteSource& source);
};

} // namespace xblob
