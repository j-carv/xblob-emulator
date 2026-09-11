#include "xblob/formats/xbe.hpp"

#include "xblob/common/safe_math.hpp"
#include "xblob/io/binary_reader.hpp"

namespace xblob {

Result<bool> XbeParser::IsXbe(const ByteSource& source) {
    if (source.size() < 4) {
        return false;
    }
    BinaryReader reader(source);
    auto magic = reader.ReadU32LE();
    if (!magic) {
        return false;
    }
    return (*magic == kXbeMagic);
}

Result<XbeInfo> XbeParser::Parse(const ByteSource& source) {
    if (source.size() < 0x14C) {
        return Error{ErrorCode::TruncatedData,
                     "Arquivo é menor que o cabeçalho mínimo obrigatório de XBE", source.size()};
    }

    BinaryReader reader(source);

    // Read and validate magic
    auto magic_res = reader.ReadU32LE();
    if (!magic_res) {
        return magic_res.error();
    }
    if (*magic_res != kXbeMagic) {
        return Error{ErrorCode::InvalidMagic, "Assinatura mágica XBEH não encontrada", 0};
    }

    XbeInfo info{};
    info.header.magic[0] = static_cast<u8>(*magic_res & 0xFF);
    info.header.magic[1] = static_cast<u8>((*magic_res >> 8) & 0xFF);
    info.header.magic[2] = static_cast<u8>((*magic_res >> 16) & 0xFF);
    info.header.magic[3] = static_cast<u8>((*magic_res >> 24) & 0xFF);

    // Signature (256 bytes)
    auto sig_res = reader.ReadExact(std::span<u8>(info.header.signature));
    if (!sig_res) {
        return sig_res.error();
    }

    // Header fields
    auto base_addr_res = reader.ReadU32LE();
    if (!base_addr_res)
        return base_addr_res.error();
    info.header.base_address = *base_addr_res;

    auto headers_size_res = reader.ReadU32LE();
    if (!headers_size_res)
        return headers_size_res.error();
    info.header.headers_size = *headers_size_res;

    auto img_size_res = reader.ReadU32LE();
    if (!img_size_res)
        return img_size_res.error();
    info.header.image_size = *img_size_res;

    auto img_hdr_size_res = reader.ReadU32LE();
    if (!img_hdr_size_res)
        return img_hdr_size_res.error();
    info.header.image_header_size = *img_hdr_size_res;

    auto ts_res = reader.ReadU32LE();
    if (!ts_res)
        return ts_res.error();
    info.header.timestamp = *ts_res;

    auto cert_addr_res = reader.ReadU32LE();
    if (!cert_addr_res)
        return cert_addr_res.error();
    info.header.certificate_address = *cert_addr_res;

    auto sec_count_res = reader.ReadU32LE();
    if (!sec_count_res)
        return sec_count_res.error();
    info.header.section_count = *sec_count_res;

    auto sec_hdrs_addr_res = reader.ReadU32LE();
    if (!sec_hdrs_addr_res)
        return sec_hdrs_addr_res.error();
    info.header.section_headers_address = *sec_hdrs_addr_res;

    auto init_flags_res = reader.ReadU32LE();
    if (!init_flags_res)
        return init_flags_res.error();
    info.header.init_flags = *init_flags_res;

    auto entry_res = reader.ReadU32LE();
    if (!entry_res)
        return entry_res.error();
    info.header.entry_point = *entry_res;

    auto tls_addr_res = reader.ReadU32LE();
    if (!tls_addr_res)
        return tls_addr_res.error();
    info.header.tls_address = *tls_addr_res;

    auto pe_stack_res = reader.ReadU32LE();
    if (!pe_stack_res)
        return pe_stack_res.error();
    info.header.pe_stack_commit = *pe_stack_res;

    auto pe_heap_res = reader.ReadU32LE();
    if (!pe_heap_res)
        return pe_heap_res.error();
    info.header.pe_heap_reserve = *pe_heap_res;

    auto pe_heap_commit_res = reader.ReadU32LE();
    if (!pe_heap_commit_res)
        return pe_heap_commit_res.error();
    info.header.pe_heap_commit = *pe_heap_commit_res;

    auto pe_base_res = reader.ReadU32LE();
    if (!pe_base_res)
        return pe_base_res.error();
    info.header.pe_base_address = *pe_base_res;

    auto pe_img_size_res = reader.ReadU32LE();
    if (!pe_img_size_res)
        return pe_img_size_res.error();
    info.header.pe_image_size = *pe_img_size_res;

    auto pe_chksum_res = reader.ReadU32LE();
    if (!pe_chksum_res)
        return pe_chksum_res.error();
    info.header.pe_checksum = *pe_chksum_res;

    auto pe_ts_res = reader.ReadU32LE();
    if (!pe_ts_res)
        return pe_ts_res.error();
    info.header.pe_timestamp = *pe_ts_res;

    // Validate headers_size
    if (info.header.headers_size < 0x14C) {
        return Error{ErrorCode::InvalidHeader, "Tamanho declarado de cabeçalhos XBE é inválido",
                     0x108};
    }
    if (info.header.headers_size > source.size()) {
        return Error{ErrorCode::TruncatedData,
                     "Tamanho declarado de cabeçalhos excede o tamanho total do arquivo", 0x108};
    }

    // Read Certificate if present
    if (info.header.certificate_address != 0) {
        if (info.header.certificate_address < info.header.base_address) {
            return Error{ErrorCode::OutOfBounds,
                         "Endereço virtual do certificado está antes do endereço base", 0x118};
        }
        const u64 cert_offset = info.header.certificate_address - info.header.base_address;
        if (!RangeInBoundsU64(cert_offset, 4, static_cast<u64>(info.header.headers_size))) {
            return Error{ErrorCode::OutOfBounds,
                         "Offset do certificado está fora da área de cabeçalhos", cert_offset};
        }

        auto seek_cert = reader.Seek(cert_offset);
        if (!seek_cert) {
            return seek_cert.error();
        }

        auto cert_size_res = reader.ReadU32LE();
        if (!cert_size_res)
            return cert_size_res.error();
        const u32 cert_size = *cert_size_res;

        if (cert_size < 4 ||
            !RangeInBoundsU64(cert_offset, cert_size, static_cast<u64>(info.header.headers_size))) {
            return Error{ErrorCode::OutOfBounds, "Tamanho ou limites do certificado são inválidos",
                         cert_offset};
        }

        XbeCertificate cert{};
        cert.size = cert_size;

        if (cert_size >= 8) {
            auto cts = reader.ReadU32LE();
            if (cts)
                cert.timestamp = *cts;
        }
        if (cert_size >= 12) {
            auto tid = reader.ReadU32LE();
            if (tid)
                cert.title_id = *tid;
        }
        if (cert_size >= 92) {
            // Read 40 UTF-16LE characters (80 bytes)
            std::string title_str;
            for (int i = 0; i < 40; ++i) {
                auto ch = reader.ReadU16LE();
                if (!ch)
                    break;
                if (*ch == 0) {
                    // Null terminator reached, skip remainder of the 40 characters
                    for (int j = i + 1; j < 40; ++j) {
                        (void)reader.ReadU16LE();
                    }
                    break;
                }
                if (*ch < 128) {
                    title_str.push_back(static_cast<char>(*ch));
                } else {
                    title_str.push_back('?');
                }
            }
            cert.title_name = std::move(title_str);
        }

        // Read alternative title ids (16 * 4 bytes = 64 bytes)
        if (cert_size >= 156) {
            for (int i = 0; i < 16; ++i) {
                auto atid = reader.ReadU32LE();
                if (atid && *atid != 0) {
                    cert.alt_title_ids.push_back(*atid);
                }
            }
        }

        if (cert_size >= 160) {
            auto am = reader.ReadU32LE();
            if (am)
                cert.allowed_media = *am;
        }
        if (cert_size >= 164) {
            auto gr = reader.ReadU32LE();
            if (gr)
                cert.game_region = *gr;
        }
        if (cert_size >= 168) {
            auto grt = reader.ReadU32LE();
            if (grt)
                cert.game_ratings = *grt;
        }
        if (cert_size >= 172) {
            auto dn = reader.ReadU32LE();
            if (dn)
                cert.disk_number = *dn;
        }
        if (cert_size >= 176) {
            auto ver = reader.ReadU32LE();
            if (ver)
                cert.version = *ver;
        }
        if (cert_size >= 192) {
            (void)reader.ReadExact(std::span<u8>(cert.lan_key));
        }
        if (cert_size >= 208) {
            (void)reader.ReadExact(std::span<u8>(cert.sig_key));
        }

        info.certificate = std::move(cert);
    }

    // Read Sections
    if (info.header.section_count > 0) {
        if (info.header.section_count > 1024) {
            return Error{ErrorCode::InvalidField,
                         "Quantidade de seções XBE excede limite seguro (1024)", 0x11C};
        }

        if (info.header.section_headers_address < info.header.base_address) {
            return Error{ErrorCode::OutOfBounds,
                         "Endereço dos cabeçalhos de seção antes do endereço base", 0x120};
        }

        const u64 sec_hdrs_offset = info.header.section_headers_address - info.header.base_address;
        const u64 total_sec_hdrs_size = static_cast<u64>(info.header.section_count) * 56ULL;

        if (!RangeInBoundsU64(sec_hdrs_offset, total_sec_hdrs_size,
                              static_cast<u64>(info.header.headers_size))) {
            return Error{ErrorCode::OutOfBounds,
                         "Tabela de cabeçalhos de seção extrapola a área de cabeçalhos",
                         sec_hdrs_offset};
        }

        auto seek_sec = reader.Seek(sec_hdrs_offset);
        if (!seek_sec) {
            return seek_sec.error();
        }

        for (u32 i = 0; i < info.header.section_count; ++i) {
            XbeSection sec{};
            auto flags = reader.ReadU32LE();
            if (!flags)
                return flags.error();
            sec.flags = *flags;

            auto vaddr = reader.ReadU32LE();
            if (!vaddr)
                return vaddr.error();
            sec.virtual_address = *vaddr;

            auto vsize = reader.ReadU32LE();
            if (!vsize)
                return vsize.error();
            sec.virtual_size = *vsize;

            auto raddr = reader.ReadU32LE();
            if (!raddr)
                return raddr.error();
            sec.raw_address = *raddr;

            auto rsize = reader.ReadU32LE();
            if (!rsize)
                return rsize.error();
            sec.raw_size = *rsize;

            auto name_addr = reader.ReadU32LE();
            if (!name_addr)
                return name_addr.error();
            sec.section_name_address = *name_addr;

            auto name_ref = reader.ReadU32LE();
            if (!name_ref)
                return name_ref.error();
            sec.section_name_ref_count = *name_ref;

            auto head_ref = reader.ReadU32LE();
            if (!head_ref)
                return head_ref.error();
            sec.head_shared_page_ref_count = *head_ref;

            auto tail_ref = reader.ReadU32LE();
            if (!tail_ref)
                return tail_ref.error();
            sec.tail_shared_page_ref_count = *tail_ref;

            auto dig_res = reader.ReadExact(std::span<u8>(sec.digest));
            if (!dig_res)
                return dig_res.error();

            // Validate raw bounds against file size
            if (sec.raw_size > 0 &&
                !RangeInBoundsU64(static_cast<u64>(sec.raw_address), static_cast<u64>(sec.raw_size),
                                  source.size())) {
                return Error{ErrorCode::OutOfBounds,
                             "Dados brutos da seção XBE extrapolam o tamanho do arquivo",
                             sec.raw_address};
            }

            info.sections.push_back(std::move(sec));
        }

        // Second pass to resolve section names if within headers
        for (auto& sec : info.sections) {
            if (sec.section_name_address >= info.header.base_address) {
                const u64 name_offset = sec.section_name_address - info.header.base_address;
                if (name_offset < info.header.headers_size) {
                    auto name_seek = reader.Seek(name_offset);
                    if (name_seek) {
                        std::string sname;
                        for (size_t k = 0; k < 64; ++k) {
                            auto ch = reader.ReadU8();
                            if (!ch || *ch == 0)
                                break;
                            if (*ch >= 32 && *ch <= 126) {
                                sname.push_back(static_cast<char>(*ch));
                            }
                        }
                        sec.name = std::move(sname);
                    }
                }
            }
        }
    }

    return info;
}

} // namespace xblob
