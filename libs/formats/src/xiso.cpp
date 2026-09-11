#include "xblob/formats/xiso.hpp"

#include "xblob/common/safe_math.hpp"
#include "xblob/io/binary_reader.hpp"

#include <cstring>

namespace xblob {

namespace {

bool MatchesAt(const ByteSource& source, u64 offset, std::string_view expected) {
    if (!RangeInBoundsU64(offset, expected.size(), source.size())) {
        return false;
    }
    std::vector<u8> buf(expected.size());
    auto res = source.ReadAt(offset, std::span<u8>(buf));
    if (!res) {
        return false;
    }
    return std::memcmp(buf.data(), expected.data(), expected.size()) == 0;
}

} // namespace

Result<MediaType> XisoDetector::Detect(const ByteSource& source) {
    // 1. Check Trimmed XISO (at sector 32, offset 0x10000)
    if (MatchesAt(source, kXisoTrimmedDescriptorOffset, kXdvdfsMagic)) {
        return MediaType::XisoTrimmed;
    }

    // 2. Check Raw Redump XISO (at sector 0x30620)
    if (MatchesAt(source, kXisoRawDescriptorOffset, kXdvdfsMagic)) {
        return MediaType::XisoRaw;
    }

    // 3. Check Standard ISO 9660 Volume Descriptor (at sector 16, offset 0x8000)
    // Offset 0x8000 = Type 0x01, Offset 0x8001 = "CD001"
    if (source.size() >= 0x8000 + 6) {
        std::array<u8, 6> iso_buf{};
        if (source.ReadAt(0x8000, std::span<u8>(iso_buf))) {
            if (iso_buf[0] == 0x01 && std::memcmp(&iso_buf[1], "CD001", 5) == 0) {
                return MediaType::Iso9660Unsupported;
            }
        }
    }

    return MediaType::Unknown;
}

Result<XisoInfo> XisoDetector::Inspect(const ByteSource& source) {
    auto det_res = Detect(source);
    if (!det_res) {
        return det_res.error();
    }

    const MediaType variant = *det_res;
    if (variant == MediaType::Iso9660Unsupported) {
        return Error{ErrorCode::UnsupportedFormat,
                     "Imagem ISO 9660 padrão detectada; contêiner não possui estrutura de disco "
                     "Xbox (XDVDFS)",
                     0x8000};
    }

    if (variant == MediaType::Unknown) {
        return Error{ErrorCode::UnknownFormat,
                     "Assinatura estrutural de mídia Xbox não foi encontrada na imagem de disco",
                     0};
    }

    u64 desc_offset = 0;
    if (variant == MediaType::XisoTrimmed) {
        desc_offset = kXisoTrimmedDescriptorOffset;
    } else if (variant == MediaType::XisoRaw) {
        desc_offset = kXisoRawDescriptorOffset;
    }

    if (!RangeInBoundsU64(desc_offset, kSectorSize, source.size())) {
        return Error{ErrorCode::TruncatedData,
                     "Arquivo truncado antes do término do setor do descritor de volume XISO",
                     desc_offset};
    }

    BinaryReader reader(source);
    auto seek_res = reader.Seek(desc_offset);
    if (!seek_res) {
        return seek_res.error();
    }

    // Header magic
    auto magic_res = reader.ReadFixedString(kXdvdfsMagic.size());
    if (!magic_res) {
        return magic_res.error();
    }
    if (*magic_res != kXdvdfsMagic) {
        return Error{ErrorCode::InvalidMagic, "Cabeçalho mágico XDVDFS inválido no descritor",
                     desc_offset};
    }

    XisoInfo info{};
    info.variant = variant;
    info.volume_descriptor_offset = desc_offset;

    auto root_sec_res = reader.ReadU32LE();
    if (!root_sec_res)
        return root_sec_res.error();
    info.root_dir_sector = *root_sec_res;

    auto root_size_res = reader.ReadU32LE();
    if (!root_size_res)
        return root_size_res.error();
    info.root_dir_size = *root_size_res;

    auto ts_res = reader.ReadU64LE();
    if (!ts_res)
        return ts_res.error();
    info.creation_timestamp = *ts_res;

    // Check footer magic at offset 2028 (0x7EC) relative to descriptor
    const u64 footer_offset = desc_offset + 0x7EC;
    if (MatchesAt(source, footer_offset, kXdvdfsMagic)) {
        info.valid_footer_magic = true;
    } else {
        info.valid_footer_magic = false;
    }

    return info;
}

} // namespace xblob
