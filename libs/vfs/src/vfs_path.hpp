#pragma once

#include "xblob/common/result.hpp"

#include <string>
#include <string_view>

namespace xblob {

struct ParsedXboxPath {
    std::string mount_alias;
    std::string relative_path;
};

[[nodiscard]] Result<ParsedXboxPath> ParseAndNormalizeXboxPath(std::string_view raw_path);

} // namespace xblob
