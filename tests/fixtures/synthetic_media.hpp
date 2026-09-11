#pragma once

#include "xblob/common/types.hpp"

#include <string_view>
#include <vector>

namespace xblob::testing {

[[nodiscard]] std::vector<u8>
CreateValidSyntheticXbe(u32 title_id = 0x12345678, std::string_view title_name = "Synthetic Game",
                        u32 section_count = 2);

[[nodiscard]] std::vector<u8> CreateTruncatedXbe();

[[nodiscard]] std::vector<u8> CreateInvalidMagicXbe();

[[nodiscard]] std::vector<u8> CreateMaliciousOffsetXbe();

[[nodiscard]] std::vector<u8> CreateValidTrimmedXiso();

[[nodiscard]] std::vector<u8> CreateTruncatedXiso();

[[nodiscard]] std::vector<u8> CreateStandardIso9660();

[[nodiscard]] std::vector<u8> CreateRandomData(size_t size);

} // namespace xblob::testing
