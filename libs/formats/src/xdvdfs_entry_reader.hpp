#pragma once

#include "xblob/common/result.hpp"
#include "xblob/formats/xdvdfs_types.hpp"
#include "xblob/io/byte_source.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace xblob {

[[nodiscard]] bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept;
[[nodiscard]] std::string ToLowerAscii(std::string_view s);

[[nodiscard]] Result<std::vector<std::string>> SplitAndValidateXboxPath(std::string_view path);

[[nodiscard]] Result<std::vector<XdvdfsEntry>>
ParseDirectoryTableEntries(const ByteSource& source, u64 partition_base, u32 starting_sector,
                           u32 dir_size_bytes, const XdvdfsBudgetConfig& budget);

} // namespace xblob
