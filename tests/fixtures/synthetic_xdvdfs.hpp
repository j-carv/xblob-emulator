#pragma once

#include "xblob/common/types.hpp"
#include "xblob/formats/xdvdfs.hpp"
#include "xblob/io/byte_source.hpp"

#include <memory>
#include <string>
#include <vector>

namespace xblob::testing {

struct SyntheticFileEntry {
    std::string path;
    std::vector<u8> data;
};

// Builds a clean, fully valid trimmed XDVDFS image (partition at offset 0).
[[nodiscard]] std::vector<u8>
BuildValidTrimmedXdvdfsImage(const std::vector<SyntheticFileEntry>& files);

// Creates a synthetic ByteSource simulating a raw Redump ISO (partition at 0x18300000)
// without allocating 400 MB of RAM.
[[nodiscard]] std::shared_ptr<ByteSource>
CreateSyntheticRawXdvdfsSource(const std::vector<SyntheticFileEntry>& files);

// Specific test scenarios
[[nodiscard]] std::vector<u8> CreateXdvdfsWithBstCycle();
[[nodiscard]] std::vector<u8> CreateXdvdfsWithCrossDirectoryCycle();
[[nodiscard]] std::vector<u8> CreateXdvdfsWithDuplicateEntries();
[[nodiscard]] std::vector<u8> CreateXdvdfsWithInvalidFilenameChars();
[[nodiscard]] std::vector<u8> CreateXdvdfsWithTruncatedFileSector();
[[nodiscard]] std::vector<u8> CreateXdvdfsWithDeepBst(size_t depth);

} // namespace xblob::testing
