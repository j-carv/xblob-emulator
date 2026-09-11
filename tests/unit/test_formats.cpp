#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/formats/xbe.hpp"
#include "xblob/formats/xdvdfs.hpp"
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

TEST_CASE(TestXdvdfsMountValidTrimmed) {
    auto xbe_data = CreateValidSyntheticXbe(0x12345678, "Trimmed Disc Game", 2);
    std::vector<u8> font_data = {'F', 'O', 'N', 'T', '1', '2', '3'};

    auto img = BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", xbe_data},
        {"MEDIA/FONT.BIN", font_data},
    });

    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());
    auto vol = *vol_res;

    EXPECT_EQ(vol->info().variant, MediaType::XisoTrimmed);
    EXPECT_EQ(vol->info().partition_base_offset, 0ULL);
    EXPECT_EQ(vol->info().descriptor_offset, 65536ULL);
    EXPECT_TRUE(vol->info().valid_footer_magic);

    // List root
    auto root_entries = vol->ListDirectory("");
    EXPECT_TRUE(root_entries.has_value());
    EXPECT_EQ(root_entries->size(), 2ULL); // DEFAULT.XBE and MEDIA

    // FindEntry case-insensitive
    auto entry1 = vol->FindEntry("default.xbe");
    EXPECT_TRUE(entry1.has_value());
    EXPECT_FALSE(entry1->is_directory);
    EXPECT_EQ(entry1->file_size, static_cast<u32>(xbe_data.size()));

    auto entry2 = vol->FindEntry("MEDIA/FONT.BIN");
    EXPECT_TRUE(entry2.has_value());
    EXPECT_FALSE(entry2->is_directory);
    EXPECT_EQ(entry2->file_size, static_cast<u32>(font_data.size()));

    // OpenFile returns SubrangeByteSource
    auto open_res = vol->OpenFile("default.xbe");
    EXPECT_TRUE(open_res.has_value());
    auto sub = *open_res;
    EXPECT_EQ(sub->size(), static_cast<u64>(xbe_data.size()));

    std::vector<u8> xbe_read(xbe_data.size());
    EXPECT_TRUE(sub->ReadAt(0, xbe_read).has_value());
    EXPECT_TRUE(xbe_read == xbe_data);

    // ReadFileAt
    std::vector<u8> font_read(font_data.size());
    auto read_cnt = vol->ReadFileAt(*entry2, 0, font_read);
    EXPECT_TRUE(read_cnt.has_value());
    EXPECT_EQ(*read_cnt, font_data.size());
    EXPECT_TRUE(font_read == font_data);
}

TEST_CASE(TestXdvdfsMountValidRaw) {
    auto xbe_data = CreateValidSyntheticXbe(0x87654321, "Raw Redump Game", 2);
    auto raw_source = CreateSyntheticRawXdvdfsSource({
        {"DEFAULT.XBE", xbe_data},
    });

    auto vol_res = XdvdfsVolume::Open(raw_source);
    EXPECT_TRUE(vol_res.has_value());
    auto vol = *vol_res;

    EXPECT_EQ(vol->info().variant, MediaType::XisoRaw);
    EXPECT_EQ(vol->info().partition_base_offset, 0x18300000ULL);
    EXPECT_TRUE(vol->info().valid_footer_magic);

    auto entry = vol->FindEntry("dEfAulT.Xbe");
    EXPECT_TRUE(entry.has_value());
    EXPECT_EQ(entry->file_size, static_cast<u32>(xbe_data.size()));

    auto open_res = vol->OpenFile("default.xbe");
    EXPECT_TRUE(open_res.has_value());
    std::vector<u8> buf(4);
    EXPECT_TRUE((*open_res)->ReadAt(0, buf).has_value());
    EXPECT_EQ(buf[0], 'X');
    EXPECT_EQ(buf[1], 'B');
    EXPECT_EQ(buf[2], 'E');
    EXPECT_EQ(buf[3], 'H');
}

TEST_CASE(TestXdvdfsTraverseAllAndNested) {
    auto img = BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", {'X'}},
        {"DIR1/FILE1.TXT", {'A'}},
        {"DIR1/SUB/FILE2.TXT", {'B'}},
    });

    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());
    auto vol = *vol_res;

    auto all_entries = vol->TraverseAll();
    EXPECT_TRUE(all_entries.has_value());
    EXPECT_EQ(all_entries->size(), 5ULL); // DEFAULT.XBE, DIR1, FILE1.TXT, SUB, FILE2.TXT
}

TEST_CASE(TestXdvdfsBstCycleDetection) {
    auto img = CreateXdvdfsWithBstCycle();
    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());

    auto list_res = (*vol_res)->ListDirectory("");
    EXPECT_FALSE(list_res.has_value());
    EXPECT_EQ(list_res.error().code, ErrorCode::InvalidField);
}

TEST_CASE(TestXdvdfsCrossDirectoryCycleDetection) {
    auto img = CreateXdvdfsWithCrossDirectoryCycle();
    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());

    auto trav_res = (*vol_res)->TraverseAll();
    EXPECT_FALSE(trav_res.has_value());
    EXPECT_EQ(trav_res.error().code, ErrorCode::InvalidField);
}

TEST_CASE(TestXdvdfsDuplicateEntriesDetection) {
    auto img = CreateXdvdfsWithDuplicateEntries();
    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());

    auto list_res = (*vol_res)->ListDirectory("");
    EXPECT_FALSE(list_res.has_value());
    EXPECT_EQ(list_res.error().code, ErrorCode::InvalidField);
}

TEST_CASE(TestXdvdfsInvalidFilenameCharsDetection) {
    auto img = CreateXdvdfsWithInvalidFilenameChars();
    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());

    auto list_res = (*vol_res)->ListDirectory("");
    EXPECT_FALSE(list_res.has_value());
    EXPECT_EQ(list_res.error().code, ErrorCode::InvalidPath);
}

TEST_CASE(TestXdvdfsTruncatedSectorDetection) {
    auto img = CreateXdvdfsWithTruncatedFileSector();
    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol_res = XdvdfsVolume::Open(source);
    EXPECT_TRUE(vol_res.has_value());

    auto open_res = (*vol_res)->OpenFile("TRUNC.BIN");
    EXPECT_FALSE(open_res.has_value());
    EXPECT_EQ(open_res.error().code, ErrorCode::TruncatedData);
}

TEST_CASE(TestXdvdfsDepthBudgetExceeded) {
    auto img = CreateXdvdfsWithDeepBst(30);
    auto source = std::make_shared<MemoryByteSource>(img);

    XdvdfsBudgetConfig budget{};
    budget.max_depth = 5; // Low depth limit

    auto vol_res = XdvdfsVolume::Open(source, budget);
    EXPECT_TRUE(vol_res.has_value());

    auto list_res = (*vol_res)->ListDirectory("");
    EXPECT_FALSE(list_res.has_value());
    EXPECT_EQ(list_res.error().code, ErrorCode::LimitReached);
}

TEST_CASE(TestXdvdfsReadAtEofBehavior) {
    std::vector<u8> file_content = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    auto img = BuildValidTrimmedXdvdfsImage({
        {"TEST.DAT", file_content},
    });

    auto source = std::make_shared<MemoryByteSource>(img);
    auto vol = *XdvdfsVolume::Open(source);
    auto entry = *vol->FindEntry("TEST.DAT");

    // Partial read at end: offset 8, buffer size 5 -> should only return 2 bytes
    std::vector<u8> buf(5);
    auto read_res = vol->ReadFileAt(entry, 8, buf);
    EXPECT_TRUE(read_res.has_value());
    EXPECT_EQ(*read_res, 2ULL);
    EXPECT_EQ(buf[0], '8');
    EXPECT_EQ(buf[1], '9');

    // Read exactly at EOF -> returns 0 bytes
    auto eof_res = vol->ReadFileAt(entry, 10, buf);
    EXPECT_TRUE(eof_res.has_value());
    EXPECT_EQ(*eof_res, 0ULL);

    // Read past EOF -> returns 0 bytes
    auto past_eof = vol->ReadFileAt(entry, 20, buf);
    EXPECT_TRUE(past_eof.has_value());
    EXPECT_EQ(*past_eof, 0ULL);
}

int main() {
    return xblob::testing::RunAllTests();
}
