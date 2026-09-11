#include "tests/test_framework.hpp"
#include "xblob/io/binary_reader.hpp"
#include "xblob/io/file_source.hpp"

#include <filesystem>
#include <fstream>

using namespace xblob;

TEST_CASE(TestBinaryReaderLittleEndian) {
    std::vector<u8> bytes = {
        0x12,                                          // u8: 0x12
        0x34, 0x12,                                    // u16: 0x1234
        0x78, 0x56, 0x34, 0x12,                        // u32: 0x12345678
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 // u64: 0x0123456789ABCDEF
    };

    MemoryByteSource source(bytes);
    BinaryReader reader(source);

    auto u8_val = reader.ReadU8();
    EXPECT_TRUE(u8_val.has_value());
    EXPECT_EQ(*u8_val, 0x12);

    auto u16_val = reader.ReadU16LE();
    EXPECT_TRUE(u16_val.has_value());
    EXPECT_EQ(*u16_val, 0x1234);

    auto u32_val = reader.ReadU32LE();
    EXPECT_TRUE(u32_val.has_value());
    EXPECT_EQ(*u32_val, 0x12345678U);

    auto u64_val = reader.ReadU64LE();
    EXPECT_TRUE(u64_val.has_value());
    EXPECT_EQ(*u64_val, 0x0123456789ABCDEFULL);

    EXPECT_EQ(reader.position(), bytes.size());
    EXPECT_EQ(reader.remaining(), 0ULL);

    // Reading past EOF
    auto eof_val = reader.ReadU8();
    EXPECT_FALSE(eof_val.has_value());
    EXPECT_EQ(eof_val.error().code, ErrorCode::UnexpectedEof);
}

TEST_CASE(TestBinaryReaderSeekAndSkip) {
    std::vector<u8> bytes = {1, 2, 3, 4, 5, 6, 7, 8};
    MemoryByteSource source(bytes);
    BinaryReader reader(source);

    EXPECT_TRUE(reader.Skip(4).has_value());
    EXPECT_EQ(reader.position(), 4ULL);

    auto val = reader.ReadU8();
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 5);

    EXPECT_TRUE(reader.Seek(1).has_value());
    auto val2 = reader.ReadU8();
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 2);

    // Out of bounds seek
    auto bad_seek = reader.Seek(20);
    EXPECT_FALSE(bad_seek.has_value());
    EXPECT_EQ(bad_seek.error().code, ErrorCode::OutOfBounds);
}

TEST_CASE(TestFileSourceNonExistent) {
    auto res = FileSource::Open("/non/existent/path/xblob_test_file.bin");
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ErrorCode::FileNotFound);
}

TEST_CASE(TestFileSourceRead) {
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "xblob_io_test.bin";
    {
        std::ofstream ofs(temp_file, std::ios::binary);
        const char msg[] = "XBLOB_FILE_TEST";
        ofs.write(msg, sizeof(msg) - 1);
    }

    auto file_res = FileSource::Open(temp_file);
    EXPECT_TRUE(file_res.has_value());
    auto& fs = *file_res;
    EXPECT_EQ(fs->size(), 15ULL);

    BinaryReader reader(*fs);
    auto str_res = reader.ReadFixedString(15);
    EXPECT_TRUE(str_res.has_value());
    EXPECT_EQ(*str_res, "XBLOB_FILE_TEST");

    std::filesystem::remove(temp_file);
}

int main() {
    return xblob::testing::RunAllTests();
}
