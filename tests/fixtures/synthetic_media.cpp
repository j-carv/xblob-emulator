#include "tests/fixtures/synthetic_media.hpp"

#include "xblob/formats/xiso.hpp"

#include <cstring>

namespace xblob::testing {

namespace {

void WriteU16LE(std::vector<u8>& buf, size_t offset, u16 val) {
    if (offset + 2 <= buf.size()) {
        buf[offset + 0] = static_cast<u8>(val & 0xFF);
        buf[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
    }
}

void WriteU32LE(std::vector<u8>& buf, size_t offset, u32 val) {
    if (offset + 4 <= buf.size()) {
        buf[offset + 0] = static_cast<u8>(val & 0xFF);
        buf[offset + 1] = static_cast<u8>((val >> 8) & 0xFF);
        buf[offset + 2] = static_cast<u8>((val >> 16) & 0xFF);
        buf[offset + 3] = static_cast<u8>((val >> 24) & 0xFF);
    }
}

void WriteU64LE(std::vector<u8>& buf, size_t offset, u64 val) {
    if (offset + 8 <= buf.size()) {
        for (size_t i = 0; i < 8; ++i) {
            buf[offset + i] = static_cast<u8>((val >> (i * 8)) & 0xFF);
        }
    }
}

} // namespace

std::vector<u8> CreateValidSyntheticXbe(u32 title_id, std::string_view title_name,
                                        u32 section_count) {
    // Total size: 0x1000 headers + 2 * 0x1000 sections = 0x3000 bytes
    std::vector<u8> xbe(0x3000, 0);

    // 1. Magic "XBEH"
    xbe[0] = 'X';
    xbe[1] = 'B';
    xbe[2] = 'E';
    xbe[3] = 'H';

    const u32 base_addr = 0x00010000;
    const u32 headers_size = 0x1000;

    WriteU32LE(xbe, 0x104, base_addr);          // Base Address
    WriteU32LE(xbe, 0x108, headers_size);       // Headers Size
    WriteU32LE(xbe, 0x10C, 0x00005000);         // Image Size
    WriteU32LE(xbe, 0x110, 0x00000178);         // Image Header Size
    WriteU32LE(xbe, 0x114, 0x60000000);         // Timestamp
    WriteU32LE(xbe, 0x118, base_addr + 0x200);  // Certificate Address (offset 0x200)
    WriteU32LE(xbe, 0x11C, section_count);      // Section Count
    WriteU32LE(xbe, 0x120, base_addr + 0x400);  // Section Headers Address (offset 0x400)
    WriteU32LE(xbe, 0x124, 0x00000001);         // Init Flags
    WriteU32LE(xbe, 0x128, base_addr + 0x1000); // Entry Point

    // Certificate at offset 0x200
    const size_t cert_off = 0x200;
    WriteU32LE(xbe, cert_off + 0x00, 0x1D0);      // Cert Size
    WriteU32LE(xbe, cert_off + 0x04, 0x60000000); // Timestamp
    WriteU32LE(xbe, cert_off + 0x08, title_id);   // Title ID

    // Write UTF-16LE Title Name at cert_off + 0x0C (up to 40 chars)
    size_t name_len = std::min(title_name.size(), size_t{39});
    for (size_t i = 0; i < name_len; ++i) {
        WriteU16LE(xbe, cert_off + 0x0C + (i * 2), static_cast<u16>(title_name[i]));
    }
    WriteU16LE(xbe, cert_off + 0x0C + (name_len * 2), 0); // Null terminator

    WriteU32LE(xbe, cert_off + 0x9C, 0xFFFFFFFF); // Allowed Media
    WriteU32LE(xbe, cert_off + 0xA0, 0x00000001); // Game Region (USA/Canada)
    WriteU32LE(xbe, cert_off + 0xA4, 0x00000000); // Game Ratings
    WriteU32LE(xbe, cert_off + 0xA8, 1);          // Disk Number
    WriteU32LE(xbe, cert_off + 0xAC, 1);          // Version

    // Section 0 at offset 0x400
    if (section_count >= 1) {
        const size_t sec0 = 0x400;
        WriteU32LE(xbe, sec0 + 0x00, 0x00000004);         // Flags
        WriteU32LE(xbe, sec0 + 0x04, base_addr + 0x1000); // Virtual Address
        WriteU32LE(xbe, sec0 + 0x08, 0x1000);             // Virtual Size
        WriteU32LE(xbe, sec0 + 0x0C, 0x1000);             // Raw Address
        WriteU32LE(xbe, sec0 + 0x10, 0x1000);             // Raw Size
        WriteU32LE(xbe, sec0 + 0x14, base_addr + 0x600);  // Name Address
        // Name at 0x600: ".text\0"
        std::memcpy(&xbe[0x600], ".text", 6);
    }

    // Section 1 at offset 0x438 (56 bytes after sec0)
    if (section_count >= 2) {
        const size_t sec1 = 0x400 + 56;
        WriteU32LE(xbe, sec1 + 0x00, 0x00000002);         // Flags
        WriteU32LE(xbe, sec1 + 0x04, base_addr + 0x2000); // Virtual Address
        WriteU32LE(xbe, sec1 + 0x08, 0x1000);             // Virtual Size
        WriteU32LE(xbe, sec1 + 0x0C, 0x2000);             // Raw Address
        WriteU32LE(xbe, sec1 + 0x10, 0x1000);             // Raw Size
        WriteU32LE(xbe, sec1 + 0x14, base_addr + 0x610);  // Name Address
        // Name at 0x610: ".rdata\0"
        std::memcpy(&xbe[0x610], ".rdata", 7);
    }

    return xbe;
}

std::vector<u8> CreateTruncatedXbe() {
    std::vector<u8> xbe(128, 0);
    xbe[0] = 'X';
    xbe[1] = 'B';
    xbe[2] = 'E';
    xbe[3] = 'H';
    return xbe;
}

std::vector<u8> CreateInvalidMagicXbe() {
    std::vector<u8> xbe(0x1000, 0);
    xbe[0] = 'B';
    xbe[1] = 'A';
    xbe[2] = 'D';
    xbe[3] = '!';
    return xbe;
}

std::vector<u8> CreateMaliciousOffsetXbe() {
    auto xbe = CreateValidSyntheticXbe();
    // Tamper section raw address to point to 0xFFFFFF00 with large size to trigger overflow/out of
    // bounds
    const size_t sec0 = 0x400;
    WriteU32LE(xbe, sec0 + 0x0C, 0xFFFFFF00); // raw_address
    WriteU32LE(xbe, sec0 + 0x10, 0x00001000); // raw_size
    return xbe;
}

std::vector<u8> CreateValidTrimmedXiso() {
    // 34 sectors = 34 * 2048 = 69,632 bytes
    std::vector<u8> iso(34 * kSectorSize, 0);
    const size_t vd_offset = kXisoTrimmedDescriptorOffset; // 0x10000 = 65,536

    // Header Magic (20 bytes)
    std::memcpy(&iso[vd_offset], kXdvdfsMagic.data(), kXdvdfsMagic.size());

    // Root dir sector at vd_offset + 20
    WriteU32LE(iso, vd_offset + 20, 33);
    // Root dir size at vd_offset + 24
    WriteU32LE(iso, vd_offset + 24, 2048);
    // Timestamp at vd_offset + 28
    WriteU64LE(iso, vd_offset + 28, 0x01D8ABCD12345678ULL);

    // Footer Magic at vd_offset + 0x7EC (offset 2028)
    std::memcpy(&iso[vd_offset + 0x7EC], kXdvdfsMagic.data(), kXdvdfsMagic.size());

    return iso;
}

std::vector<u8> CreateTruncatedXiso() {
    // Shorter than 0x10000 + 2048
    std::vector<u8> iso(0x10000 + 10, 0);
    std::memcpy(&iso[0x10000], kXdvdfsMagic.data(), 10);
    return iso;
}

std::vector<u8> CreateStandardIso9660() {
    // Sector 16 at offset 0x8000
    std::vector<u8> iso(18 * kSectorSize, 0);
    iso[0x8000] = 0x01; // Primary Volume Descriptor
    std::memcpy(&iso[0x8001], "CD001", 5);
    return iso;
}

std::vector<u8> CreateRandomData(size_t size) {
    std::vector<u8> data(size);
    u32 state = 0x12345678;
    for (size_t i = 0; i < size; ++i) {
        state = state * 1103515245 + 12345;
        data[i] = static_cast<u8>((state >> 16) & 0xFF);
    }
    return data;
}

} // namespace xblob::testing
