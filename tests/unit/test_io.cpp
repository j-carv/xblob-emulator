#include "tests/test_framework.hpp"
#include "xblob/io/binary_reader.hpp"
#include "xblob/io/file_source.hpp"
#include "xblob/io/subrange_byte_source.hpp"

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

TEST_CASE(TestSubrangeByteSourceBasicAndBounds) {
    std::vector<u8> data = {'H', 'E', 'L', 'L', 'O', ' ', 'W', 'O', 'R', 'L', 'D', '!'};
    auto parent = std::make_shared<MemoryByteSource>(data);

    // Valid subrange: "WORLD" (offset 6, size 5)
    auto sub = SubrangeByteSource::Create(parent, 6, 5);
    EXPECT_TRUE(sub.has_value());
    auto& s = *sub;
    EXPECT_EQ(s->size(), 5ULL);
    EXPECT_EQ(s->base_offset(), 6ULL);

    std::vector<u8> buf(5);
    EXPECT_TRUE(s->ReadAt(0, buf).has_value());
    EXPECT_EQ(std::string(buf.begin(), buf.end()), "WORLD");

    // Partial read within bounds
    std::vector<u8> partial(3);
    EXPECT_TRUE(s->ReadAt(1, partial).has_value());
    EXPECT_EQ(std::string(partial.begin(), partial.end()), "ORL");

    // Out of bounds read past subrange size
    std::vector<u8> overflow_buf(6);
    auto bad_read = s->ReadAt(0, overflow_buf);
    EXPECT_FALSE(bad_read.has_value());
    EXPECT_EQ(bad_read.error().code, ErrorCode::OutOfBounds);

    auto bad_read2 = s->ReadAt(4, partial);
    EXPECT_FALSE(bad_read2.has_value());
    EXPECT_EQ(bad_read2.error().code, ErrorCode::OutOfBounds);

    // Empty read succeeds
    EXPECT_TRUE(s->ReadAt(0, std::span<u8>{}).has_value());

    // SpanAt within bounds
    auto span_res = s->SpanAt(2, 2);
    EXPECT_TRUE(span_res.has_value());
    EXPECT_EQ((*span_res)[0], 'R');
    EXPECT_EQ((*span_res)[1], 'L');

    // SpanAt out of bounds
    auto bad_span = s->SpanAt(4, 2);
    EXPECT_FALSE(bad_span.has_value());
    EXPECT_EQ(bad_span.error().code, ErrorCode::OutOfBounds);
}

TEST_CASE(TestSubrangeByteSourceCreationValidation) {
    std::vector<u8> data = {1, 2, 3, 4};
    auto parent = std::make_shared<MemoryByteSource>(data);

    // Null parent
    auto null_res = SubrangeByteSource::Create(nullptr, 0, 2);
    EXPECT_FALSE(null_res.has_value());
    EXPECT_EQ(null_res.error().code, ErrorCode::InvalidArgument);

    // Past parent size
    auto out_of_bounds = SubrangeByteSource::Create(parent, 2, 3);
    EXPECT_FALSE(out_of_bounds.has_value());
    EXPECT_EQ(out_of_bounds.error().code, ErrorCode::OutOfBounds);

    // Exact end boundary is valid
    auto boundary = SubrangeByteSource::Create(parent, 2, 2);
    EXPECT_TRUE(boundary.has_value());
    EXPECT_EQ((*boundary)->size(), 2ULL);

    // Integer overflow in creation offset + size
    auto overflow = SubrangeByteSource::Create(parent, UINT64_MAX, 1);
    EXPECT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error().code, ErrorCode::OutOfBounds);
}

TEST_CASE(TestSubrangeByteSourceLifetimeAndNesting) {
    std::shared_ptr<SubrangeByteSource> sub;
    {
        std::vector<u8> parent_data = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'};
        auto parent_source = std::make_shared<VectorByteSource>(std::move(parent_data));
        auto create_res = SubrangeByteSource::Create(parent_source, 2, 6); // C, D, E, F, G, H
        EXPECT_TRUE(create_res.has_value());
        sub = *create_res;
        // parent_source dropped from local scope here
    }

    // sub still holds shared_ptr to parent
    std::vector<u8> buf(4);
    EXPECT_TRUE(sub->ReadAt(0, buf).has_value());
    EXPECT_EQ(buf[0], 'C');
    EXPECT_EQ(buf[1], 'D');
    EXPECT_EQ(buf[2], 'E');
    EXPECT_EQ(buf[3], 'F');

    // Nested subrange
    auto nested_res = SubrangeByteSource::Create(sub, 2, 3); // E, F, G
    EXPECT_TRUE(nested_res.has_value());
    auto nested = *nested_res;
    EXPECT_EQ(nested->size(), 3ULL);
    std::vector<u8> nested_buf(3);
    EXPECT_TRUE(nested->ReadAt(0, nested_buf).has_value());
    EXPECT_EQ(nested_buf[0], 'E');
    EXPECT_EQ(nested_buf[1], 'F');
    EXPECT_EQ(nested_buf[2], 'G');
}

int main() {
    return xblob::testing::RunAllTests();
}
