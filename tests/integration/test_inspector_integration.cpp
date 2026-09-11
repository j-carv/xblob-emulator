#include "tests/fixtures/synthetic_media.hpp"
#include "tests/test_framework.hpp"
#include "xblob/formats/inspector.hpp"

#include <filesystem>
#include <fstream>

using namespace xblob;
using namespace xblob::testing;

TEST_CASE(TestMediaInspectorWithXbeMemory) {
    auto data = CreateValidSyntheticXbe(0x12345678, "Integration Game", 2);
    MemoryByteSource source(data);

    auto report = MediaInspector::Inspect(source, "memory_test.xbe");
    EXPECT_TRUE(report.has_value());
    EXPECT_EQ(report->type, MediaType::XbeExecutable);
    EXPECT_TRUE(report->xbe.has_value());
    EXPECT_FALSE(report->xiso.has_value());

    std::string text = report->FormatHumanReadable();
    EXPECT_NE(text.find("Executável Xbox (XBE)"), std::string::npos);
    EXPECT_NE(text.find("Integration Game"), std::string::npos);
    EXPECT_NE(text.find(".text"), std::string::npos);
}

TEST_CASE(TestMediaInspectorWithXisoMemory) {
    auto data = CreateValidTrimmedXiso();
    MemoryByteSource source(data);

    auto report = MediaInspector::Inspect(source, "disc.iso");
    EXPECT_TRUE(report.has_value());
    EXPECT_EQ(report->type, MediaType::XisoTrimmed);
    EXPECT_TRUE(report->xiso.has_value());
    EXPECT_FALSE(report->xbe.has_value());

    std::string text = report->FormatHumanReadable();
    EXPECT_NE(text.find("XISO Otimizada/Trimmed"), std::string::npos);
    EXPECT_NE(text.find("MICROSOFT*XBOX*MEDIA"), std::string::npos);
}

TEST_CASE(TestMediaInspectorStandardIsoUnsupported) {
    auto data = CreateStandardIso9660();
    MemoryByteSource source(data);

    auto report = MediaInspector::Inspect(source, "standard_pc.iso");
    EXPECT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::UnsupportedFormat);
}

TEST_CASE(TestMediaInspectorRandomNoiseUnknown) {
    auto data = CreateRandomData(50000);
    MemoryByteSource source(data);

    auto report = MediaInspector::Inspect(source, "random.bin");
    EXPECT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ErrorCode::UnknownFormat);
}

TEST_CASE(TestMediaInspectorFileIO) {
    std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "xblob_integration_test";
    std::filesystem::create_directories(temp_dir);

    std::filesystem::path xbe_path = temp_dir / "default.xbe";
    {
        auto data = CreateValidSyntheticXbe(0xAABBCCDD, "Disk Fixture Game", 1);
        std::ofstream ofs(xbe_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }

    auto file_report = MediaInspector::InspectFile(xbe_path);
    EXPECT_TRUE(file_report.has_value());
    EXPECT_EQ(file_report->type, MediaType::XbeExecutable);
    EXPECT_TRUE(file_report->xbe.has_value());
    EXPECT_EQ(file_report->xbe->certificate->title_id, 0xAABBCCDD);

    // Non-existent file test
    auto missing_report = MediaInspector::InspectFile(temp_dir / "non_existent.xbe");
    EXPECT_FALSE(missing_report.has_value());
    EXPECT_EQ(missing_report.error().code, ErrorCode::FileNotFound);

    // Deceptive extension test: random noise in game.iso
    std::filesystem::path fake_iso = temp_dir / "fake_game.iso";
    {
        auto data = CreateRandomData(80000);
        std::ofstream ofs(fake_iso, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }

    auto fake_report = MediaInspector::InspectFile(fake_iso);
    EXPECT_FALSE(fake_report.has_value());
    EXPECT_EQ(fake_report.error().code, ErrorCode::UnknownFormat);

    std::filesystem::remove_all(temp_dir);
}

int main() {
    return xblob::testing::RunAllTests();
}
