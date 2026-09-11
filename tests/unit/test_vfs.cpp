#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/vfs/vfs.hpp"
#include "xblob/vfs/xdvdfs_vfs_volume.hpp"

using namespace xblob;

namespace {

std::shared_ptr<IVfsVolume> CreateTestXdvdfsVolume() {
    auto xbe_data = testing::CreateValidSyntheticXbe(0x11223344, "VFS Test Game", 2);
    std::vector<u8> config_data = {'C', 'F', 'G', '=', '1'};

    auto img = testing::BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", xbe_data},
        {"SYSTEM/CONFIG.INI", config_data},
    });

    auto source = std::make_shared<VectorByteSource>(std::move(img));
    auto vol_res = XdvdfsVolume::Open(source);
    if (!vol_res) {
        return nullptr;
    }
    return std::make_shared<XdvdfsVfsVolume>(*vol_res);
}

} // namespace

TEST_CASE(TestVfsMountAndQuery) {
    auto vol = CreateTestXdvdfsVolume();
    EXPECT_TRUE(vol != nullptr);

    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());
    EXPECT_TRUE(vfs.IsMounted("d"));
    EXPECT_TRUE(vfs.IsMounted("D:"));

    auto info_res = vfs.QueryPath("D:\\default.xbe");
    EXPECT_TRUE(info_res.has_value());
    EXPECT_FALSE(info_res->is_directory);
    EXPECT_TRUE(info_res->size > 0);

    auto dev_info = vfs.QueryPath("\\Device\\CdRom0\\system\\config.ini");
    EXPECT_TRUE(dev_info.has_value());
    EXPECT_FALSE(dev_info->is_directory);
    EXPECT_EQ(dev_info->size, 5ULL);
}

TEST_CASE(TestVfsPathNormalizationAndTraversalRejection) {
    auto vol = CreateTestXdvdfsVolume();
    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());

    // Traversal rejection
    auto trav1 = vfs.OpenFile("D:\\..\\host.txt");
    EXPECT_FALSE(trav1.has_value());
    EXPECT_EQ(trav1.error().code, ErrorCode::InvalidPath);

    auto trav2 = vfs.OpenFile("D:\\system\\..\\default.xbe");
    EXPECT_FALSE(trav2.has_value());
    EXPECT_EQ(trav2.error().code, ErrorCode::InvalidPath);

    auto trav3 = vfs.OpenFile("D:\\.\\default.xbe");
    EXPECT_FALSE(trav3.has_value());
    EXPECT_EQ(trav3.error().code, ErrorCode::InvalidPath);

    // Invalid characters
    auto bad_char = vfs.OpenFile("D:\\file*name.bin");
    EXPECT_FALSE(bad_char.has_value());
    EXPECT_EQ(bad_char.error().code, ErrorCode::InvalidPath);

    // Unmounted drive
    auto unmounted = vfs.OpenFile("E:\\file.bin");
    EXPECT_FALSE(unmounted.has_value());
    EXPECT_EQ(unmounted.error().code, ErrorCode::FileNotFound);
}

TEST_CASE(TestVfsGenerationalHandlesAndStaleDetection) {
    auto vol = CreateTestXdvdfsVolume();
    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());

    // Open file
    auto h1_res = vfs.OpenFile("D:\\system\\config.ini");
    EXPECT_TRUE(h1_res.has_value());
    VfsHandle h1 = *h1_res;
    EXPECT_TRUE(h1.IsValid());

    // Close handle
    EXPECT_TRUE(vfs.Close(h1).has_value());

    // Double close must fail
    auto double_close = vfs.Close(h1);
    EXPECT_FALSE(double_close.has_value());
    EXPECT_EQ(double_close.error().code, ErrorCode::InvalidHandle);

    // Stale handle read must fail
    std::vector<u8> buf(5);
    auto read_stale = vfs.Read(h1, buf);
    EXPECT_FALSE(read_stale.has_value());
    EXPECT_EQ(read_stale.error().code, ErrorCode::InvalidHandle);

    // Re-open in same slot: generation should advance
    auto h2_res = vfs.OpenFile("D:\\system\\config.ini");
    EXPECT_TRUE(h2_res.has_value());
    VfsHandle h2 = *h2_res;

    // Slot index might be reused, but generation must differ
    if (h1.index == h2.index) {
        EXPECT_TRUE(h1.generation != h2.generation);
    }

    // Attempting to read with h1 still fails
    auto read_old = vfs.Read(h1, buf);
    EXPECT_FALSE(read_old.has_value());
    EXPECT_EQ(read_old.error().code, ErrorCode::InvalidHandle);

    // Read with h2 succeeds
    auto read_new = vfs.Read(h2, buf);
    EXPECT_TRUE(read_new.has_value());
    EXPECT_EQ(*read_new, 5ULL);

    EXPECT_TRUE(vfs.Close(h2).has_value());
}

TEST_CASE(TestVfsTransactionalReadAndSeek) {
    auto vol = CreateTestXdvdfsVolume();
    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());

    auto h_res = vfs.OpenFile("D:\\system\\config.ini");
    EXPECT_TRUE(h_res.has_value());
    VfsHandle h = *h_res;

    std::vector<u8> buf(3);
    auto r1 = vfs.Read(h, buf);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, 3ULL);
    EXPECT_EQ(buf[0], 'C');
    EXPECT_EQ(buf[1], 'F');
    EXPECT_EQ(buf[2], 'G');

    auto pos1 = vfs.GetPosition(h);
    EXPECT_TRUE(pos1.has_value());
    EXPECT_EQ(*pos1, 3ULL);

    // Seeking before 0 must fail and NOT alter position
    auto seek_bad = vfs.Seek(h, -10, VfsSeekOrigin::Current);
    EXPECT_FALSE(seek_bad.has_value());
    EXPECT_EQ(seek_bad.error().code, ErrorCode::InvalidArgument);

    auto pos_after_bad_seek = vfs.GetPosition(h);
    EXPECT_TRUE(pos_after_bad_seek.has_value());
    EXPECT_EQ(*pos_after_bad_seek, 3ULL);

    // Seek to beginning
    auto seek_beg = vfs.Seek(h, 0, VfsSeekOrigin::Begin);
    EXPECT_TRUE(seek_beg.has_value());
    EXPECT_EQ(*seek_beg, 0ULL);

    std::vector<u8> all_buf(5);
    auto r2 = vfs.Read(h, all_buf);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, 5ULL);

    // Read at EOF returns 0 bytes
    auto r_eof = vfs.Read(h, buf);
    EXPECT_TRUE(r_eof.has_value());
    EXPECT_EQ(*r_eof, 0ULL);

    EXPECT_TRUE(vfs.Close(h).has_value());
}

TEST_CASE(TestVfsDirectoryEnumeration) {
    auto vol = CreateTestXdvdfsVolume();
    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());

    auto dir_res = vfs.OpenDirectory("D:\\");
    EXPECT_TRUE(dir_res.has_value());
    VfsHandle dir_h = *dir_res;

    std::vector<VfsDirEntry> entries;
    auto enum_res = vfs.EnumerateDirectory(dir_h, entries);
    EXPECT_TRUE(enum_res.has_value());
    EXPECT_EQ(entries.size(), 2ULL); // DEFAULT.XBE and SYSTEM

    EXPECT_TRUE(vfs.Close(dir_h).has_value());
}

TEST_CASE(TestVfsReadOnlyRejection) {
    auto vol = CreateTestXdvdfsVolume();
    Vfs vfs;
    EXPECT_TRUE(vfs.Mount("D", vol).has_value());

    // Open for write must fail
    auto open_w = vfs.OpenFile("D:\\file.txt", VfsAccessMode::Write);
    EXPECT_FALSE(open_w.has_value());
    EXPECT_EQ(open_w.error().code, ErrorCode::ReadOnlyFileSystem);

    // Create file must fail
    auto create_f = vfs.CreateFile("D:\\new.txt");
    EXPECT_FALSE(create_f.has_value());
    EXPECT_EQ(create_f.error().code, ErrorCode::ReadOnlyFileSystem);

    // Write must fail
    u8 b = 0xAA;
    VfsHandle dummy_handle{.index = 0, .generation = 1};
    auto write_res = vfs.Write(dummy_handle, std::span<const u8>(&b, 1));
    EXPECT_FALSE(write_res.has_value());
    EXPECT_EQ(write_res.error().code, ErrorCode::ReadOnlyFileSystem);

    // Delete must fail
    auto del_res = vfs.DeleteFile("D:\\default.xbe");
    EXPECT_FALSE(del_res.has_value());
    EXPECT_EQ(del_res.error().code, ErrorCode::ReadOnlyFileSystem);
}

TEST_CASE(TestVfsSessionIsolationAndTeardown) {
    auto vol1 = CreateTestXdvdfsVolume();
    auto vol2 = CreateTestXdvdfsVolume();

    Vfs session1;
    Vfs session2;

    EXPECT_TRUE(session1.Mount("D", vol1).has_value());
    EXPECT_TRUE(session2.Mount("D", vol2).has_value());

    auto h1 = *session1.OpenFile("D:\\system\\config.ini");
    auto h2 = *session2.OpenFile("D:\\system\\config.ini");

    // Advance session 1 position
    std::vector<u8> b1(2);
    EXPECT_TRUE(session1.Read(h1, b1).has_value());
    EXPECT_EQ(*session1.GetPosition(h1), 2ULL);

    // Session 2 position must remain at 0
    EXPECT_EQ(*session2.GetPosition(h2), 0ULL);

    // Teardown session 1
    session1.Reset();
    EXPECT_EQ(session1.handle_table().OpenCount(), 0ULL);
    EXPECT_FALSE(session1.IsMounted("D"));

    // Session 2 remains intact
    EXPECT_EQ(session2.handle_table().OpenCount(), 1ULL);
    EXPECT_TRUE(session2.IsMounted("D"));
    EXPECT_TRUE(session2.Close(h2).has_value());
}

int main() {
    return xblob::testing::RunAllTests();
}
