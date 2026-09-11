#include "tests/fixtures/synthetic_media.hpp"
#include "tests/test_framework.hpp"
#include "xblob/formats/xbe.hpp"
#include "xblob/formats/xiso.hpp"
#include "xblob/io/byte_source.hpp"

using namespace xblob;
using namespace xblob::testing;

TEST_CASE(TestXbeValidSynthetic) {
    auto data = CreateValidSyntheticXbe(0x54540001, "Halo Test Synthetic", 2);
    MemoryByteSource source(data);

    auto is_xbe = XbeParser::IsXbe(source);
    EXPECT_TRUE(is_xbe.has_value());
    EXPECT_TRUE(*is_xbe);

    auto info = XbeParser::Parse(source);
    EXPECT_TRUE(info.has_value());
    EXPECT_EQ(info->header.base_address, 0x00010000U);
    EXPECT_EQ(info->header.headers_size, 0x1000U);
    EXPECT_EQ(info->header.section_count, 2U);

    EXPECT_TRUE(info->certificate.has_value());
    EXPECT_EQ(info->certificate->title_id, 0x54540001U);
    EXPECT_EQ(info->certificate->title_name, "Halo Test Synthetic");

    EXPECT_EQ(info->sections.size(), 2ULL);
    EXPECT_EQ(info->sections[0].name, ".text");
    EXPECT_EQ(info->sections[0].raw_address, 0x1000U);
    EXPECT_EQ(info->sections[0].raw_size, 0x1000U);
    EXPECT_EQ(info->sections[1].name, ".rdata");
}

TEST_CASE(TestXbeTruncatedData) {
    auto data = CreateTruncatedXbe();
    MemoryByteSource source(data);

    auto info = XbeParser::Parse(source);
    EXPECT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::TruncatedData);
}

TEST_CASE(TestXbeInvalidMagic) {
    auto data = CreateInvalidMagicXbe();
    MemoryByteSource source(data);

    auto is_xbe = XbeParser::IsXbe(source);
    EXPECT_TRUE(is_xbe.has_value());
    EXPECT_FALSE(*is_xbe);

    auto info = XbeParser::Parse(source);
    EXPECT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::InvalidMagic);
}

TEST_CASE(TestXbeMaliciousOffset) {
    auto data = CreateMaliciousOffsetXbe();
    MemoryByteSource source(data);

    auto info = XbeParser::Parse(source);
    EXPECT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::OutOfBounds);
}

TEST_CASE(TestXisoValidTrimmed) {
    auto data = CreateValidTrimmedXiso();
    MemoryByteSource source(data);

    auto detected = XisoDetector::Detect(source);
    EXPECT_TRUE(detected.has_value());
    EXPECT_EQ(*detected, MediaType::XisoTrimmed);

    auto info = XisoDetector::Inspect(source);
    EXPECT_TRUE(info.has_value());
    EXPECT_EQ(info->variant, MediaType::XisoTrimmed);
    EXPECT_EQ(info->volume_descriptor_offset, 65536ULL);
    EXPECT_EQ(info->root_dir_sector, 33U);
    EXPECT_EQ(info->root_dir_size, 2048U);
    EXPECT_TRUE(info->valid_footer_magic);
}

TEST_CASE(TestXisoTruncated) {
    auto data = CreateTruncatedXiso();
    MemoryByteSource source(data);

    auto detected = XisoDetector::Detect(source);
    EXPECT_TRUE(detected.has_value());
    // Truncated during descriptor, should not match or fail cleanly
    EXPECT_NE(*detected, MediaType::XisoTrimmed);

    auto info = XisoDetector::Inspect(source);
    EXPECT_FALSE(info.has_value());
}

TEST_CASE(TestIso9660Unsupported) {
    auto data = CreateStandardIso9660();
    MemoryByteSource source(data);

    auto detected = XisoDetector::Detect(source);
    EXPECT_TRUE(detected.has_value());
    EXPECT_EQ(*detected, MediaType::Iso9660Unsupported);

    auto info = XisoDetector::Inspect(source);
    EXPECT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::UnsupportedFormat);
}

TEST_CASE(TestDeceptiveExtensionRejection) {
    // Random data must not be detected as valid XBE or XISO
    auto random_data = CreateRandomData(100000);
    MemoryByteSource source(random_data);

    auto is_xbe = XbeParser::IsXbe(source);
    EXPECT_TRUE(is_xbe.has_value());
    EXPECT_FALSE(*is_xbe);

    auto is_iso = XisoDetector::Detect(source);
    EXPECT_TRUE(is_iso.has_value());
    EXPECT_EQ(*is_iso, MediaType::Unknown);
}

int main() {
    return xblob::testing::RunAllTests();
}
