#pragma once

#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/io/byte_source.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace xblob {

constexpr u32 kXbeMagic = 0x48454258; // "XBEH" in little-endian

struct XbeHeader {
    std::array<u8, 4> magic{};
    std::array<u8, 256> signature{};
    u32 base_address{0};
    u32 headers_size{0};
    u32 image_size{0};
    u32 image_header_size{0};
    u32 timestamp{0};
    u32 certificate_address{0};
    u32 section_count{0};
    u32 section_headers_address{0};
    u32 init_flags{0};
    u32 entry_point{0};
    u32 tls_address{0};
    u32 pe_stack_commit{0};
    u32 pe_heap_reserve{0};
    u32 pe_heap_commit{0};
    u32 pe_base_address{0};
    u32 pe_image_size{0};
    u32 pe_checksum{0};
    u32 pe_timestamp{0};
};

struct XbeCertificate {
    u32 size{0};
    u32 timestamp{0};
    u32 title_id{0};
    std::string title_name;
    std::vector<u32> alt_title_ids;
    u32 allowed_media{0};
    u32 game_region{0};
    u32 game_ratings{0};
    u32 disk_number{0};
    u32 version{0};
    std::array<u8, 16> lan_key{};
    std::array<u8, 16> sig_key{};
};

struct XbeSection {
    std::string name;
    u32 flags{0};
    u32 virtual_address{0};
    u32 virtual_size{0};
    u32 raw_address{0};
    u32 raw_size{0};
    u32 section_name_address{0};
    u32 section_name_ref_count{0};
    u32 head_shared_page_ref_count{0};
    u32 tail_shared_page_ref_count{0};
    std::array<u8, 20> digest{};
};

struct XbeInfo {
    XbeHeader header;
    std::optional<XbeCertificate> certificate;
    std::vector<XbeSection> sections;
};

class XbeParser {
public:
    [[nodiscard]] static Result<bool> IsXbe(const ByteSource& source);
    [[nodiscard]] static Result<XbeInfo> Parse(const ByteSource& source);
};

} // namespace xblob
