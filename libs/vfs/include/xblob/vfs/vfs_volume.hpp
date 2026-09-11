#pragma once

#include "xblob/common/result.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/vfs/vfs_types.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace xblob {

class IVfsVolume {
public:
    virtual ~IVfsVolume() = default;

    [[nodiscard]] virtual bool IsReadOnly() const noexcept = 0;
    [[nodiscard]] virtual Result<VfsFileInfo> QueryInfo(std::string_view relative_path) = 0;
    [[nodiscard]] virtual Result<std::shared_ptr<const ByteSource>>
    OpenFile(std::string_view relative_path) = 0;
    [[nodiscard]] virtual Result<std::vector<VfsDirEntry>>
    ListDirectory(std::string_view relative_path) = 0;
};

} // namespace xblob
