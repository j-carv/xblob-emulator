#pragma once

#include "xblob/common/result.hpp"
#include "xblob/formats/xdvdfs_types.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/io/subrange_byte_source.hpp"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xblob {

class XdvdfsVolume {
public:
    [[nodiscard]] static Result<std::shared_ptr<XdvdfsVolume>>
    Open(std::shared_ptr<const ByteSource> source, XdvdfsBudgetConfig budget = {});

    virtual ~XdvdfsVolume() = default;

    [[nodiscard]] const XdvdfsVolumeInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::shared_ptr<const ByteSource>& source() const noexcept {
        return source_;
    }
    [[nodiscard]] const XdvdfsBudgetConfig& budget() const noexcept { return budget_; }

    [[nodiscard]] Result<std::vector<XdvdfsEntry>> ListDirectory(std::string_view path) const;
    [[nodiscard]] Result<XdvdfsEntry> FindEntry(std::string_view path) const;
    [[nodiscard]] Result<std::shared_ptr<SubrangeByteSource>> OpenFile(std::string_view path) const;
    [[nodiscard]] Result<std::shared_ptr<SubrangeByteSource>>
    OpenFile(const XdvdfsEntry& entry) const;
    [[nodiscard]] Result<size_t> ReadFileAt(const XdvdfsEntry& entry, u64 offset,
                                            std::span<u8> dst) const;

    [[nodiscard]] Result<std::vector<std::pair<std::string, XdvdfsEntry>>>
    TraverseAll(std::string_view start_path = "") const;

private:
    XdvdfsVolume(std::shared_ptr<const ByteSource> source, XdvdfsVolumeInfo info,
                 XdvdfsBudgetConfig budget) noexcept;

    Result<std::vector<XdvdfsEntry>> ParseDirectoryTable(u32 starting_sector,
                                                         u32 dir_size_bytes) const;

    std::shared_ptr<const ByteSource> source_;
    XdvdfsVolumeInfo info_{};
    XdvdfsBudgetConfig budget_{};
};

} // namespace xblob
