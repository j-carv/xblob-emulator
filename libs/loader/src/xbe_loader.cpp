#include "xblob/loader/xbe_loader.hpp"

#include "xblob/formats/xbe.hpp"

#include <algorithm>
#include <map>
#include <vector>

namespace xblob::loader {

namespace {

struct MemoryPatch {
    GuestAddr addr{0};
    std::vector<u8> original;
};

void RollbackPatches(memory::AddressSpace& space,
                     const std::vector<MemoryPatch>& patches) noexcept {
    for (auto it = patches.rbegin(); it != patches.rend(); ++it) {
        (void)space.WriteBytes(it->addr, it->original);
    }
}

} // namespace

Result<XbeLoadPlan> XbeLoader::Plan(const ByteSource& source) {
    auto parsed = XbeParser::Parse(source);
    if (!parsed) {
        return parsed.error();
    }

    const auto& xbe = *parsed;
    const auto& hdr = xbe.header;

    // Explicit rejection of out-of-scope TLS
    if (hdr.tls_address != 0) {
        return Error{ErrorCode::UnsupportedFeature, "TLS não suportado na fundação do loader XBE",
                     hdr.tls_address};
    }

    // Explicit validation of header fields
    if (hdr.headers_size == 0) {
        return Error{ErrorCode::InvalidField, "Tamanho dos cabeçalhos não pode ser zero", 0};
    }
    if (hdr.image_size == 0) {
        return Error{ErrorCode::InvalidField, "Tamanho da imagem não pode ser zero", 0};
    }
    if (hdr.headers_size > hdr.image_size) {
        return Error{ErrorCode::OutOfBounds,
                     "Tamanho dos cabeçalhos excede o tamanho total da imagem", hdr.headers_size};
    }
    if (hdr.headers_size > source.size()) {
        return Error{ErrorCode::TruncatedData,
                     "Tamanho dos cabeçalhos ultrapassa o tamanho do arquivo", hdr.headers_size};
    }

    const u64 img_start = hdr.base_address;
    const u64 img_end = img_start + static_cast<u64>(hdr.image_size);
    if (img_end > 0x100000000ULL) {
        return Error{ErrorCode::IntegerOverflow,
                     "Imagem extrapola espaço de endereçamento de 32 bits", hdr.base_address};
    }

    // Validate entry point falls within image bounds
    if (hdr.entry_point < hdr.base_address || hdr.entry_point >= img_end) {
        return Error{ErrorCode::OutOfBounds, "Entry point fora dos limites da imagem",
                     hdr.entry_point};
    }

    const u64 headers_start = hdr.base_address;
    const u64 headers_end = headers_start + static_cast<u64>(hdr.headers_size);

    XbeLoadPlan plan;
    plan.base_address = hdr.base_address;
    plan.headers_size = hdr.headers_size;
    plan.image_size = hdr.image_size;
    plan.entry_point = hdr.entry_point;
    plan.headers_permissions = memory::MemoryPermission::Read;

    for (const auto& sec : xbe.sections) {
        // Rejection of TLS sections
        if (sec.name == ".tls") {
            return Error{ErrorCode::UnsupportedFeature,
                         "Seção TLS fora do escopo do loader de fundação"};
        }

        // Validate unsupported section flags
        constexpr u32 kSupportedFlagsMask = 0x3Fu; // bits 0..5
        if ((sec.flags & ~kSupportedFlagsMask) != 0) {
            return Error{ErrorCode::UnsupportedFeature, "Flags de seção XBE não suportadas",
                         sec.flags};
        }

        if (sec.raw_size > sec.virtual_size) {
            return Error{ErrorCode::OutOfBounds, "raw_size de seção não pode exceder virtual_size",
                         sec.virtual_address};
        }

        const u64 raw_file_end = static_cast<u64>(sec.raw_address) + static_cast<u64>(sec.raw_size);
        if (raw_file_end > source.size()) {
            return Error{ErrorCode::TruncatedData,
                         "Dados brutos da seção ultrapassam tamanho do arquivo", sec.raw_address};
        }

        const u64 sec_v_start = sec.virtual_address;
        const u64 sec_v_end = sec_v_start + static_cast<u64>(sec.virtual_size);

        if (sec_v_start < img_start || sec_v_end > img_end) {
            return Error{ErrorCode::OutOfBounds,
                         "Seção mapeada fora dos limites declarados da imagem",
                         sec.virtual_address};
        }

        // Overlap with headers
        if (sec_v_start < headers_end && headers_start < sec_v_end) {
            return Error{ErrorCode::RegionOverlap,
                         "Seção sobrepõe a região de cabeçalhos da imagem", sec.virtual_address};
        }

        // Overlap with previously planned sections
        for (const auto& prev : plan.sections) {
            if (sec_v_start < prev.virtual_end() && prev.virtual_address < sec_v_end) {
                return Error{ErrorCode::RegionOverlap,
                             "Sobreposição entre seções consecutivas do XBE", sec.virtual_address};
            }
        }

        // Conservative permissions
        memory::MemoryPermission perms = memory::MemoryPermission::Read;
        if ((sec.flags & 0x04) != 0) {
            perms = perms | memory::MemoryPermission::Execute;
        }
        if ((sec.flags & 0x01) != 0) {
            perms = perms | memory::MemoryPermission::Write;
        }

        const GuestSize zero_fill = sec.virtual_size - sec.raw_size;

        plan.sections.push_back(SectionPlan{
            .name = sec.name,
            .virtual_address = sec.virtual_address,
            .virtual_size = sec.virtual_size,
            .raw_address = sec.raw_address,
            .raw_size = sec.raw_size,
            .flags = sec.flags,
            .permissions = perms,
            .zero_fill_size = zero_fill,
        });
    }

    // Diagnostic Eligibility Check
    bool is_synth = false;
    if (xbe.certificate.has_value()) {
        const auto& cert = *xbe.certificate;
        if (cert.title_id >= 0xFFFE0000 || cert.title_name.find("Synthetic") != std::string::npos ||
            cert.title_name.find("Diagnostic") != std::string::npos ||
            cert.title_name.find("xblob") != std::string::npos) {
            is_synth = true;
        }
    }

    if (is_synth) {
        plan.is_diagnostic_eligible = true;
        plan.diagnostic_eligibility_reason = "Binário sintético diagnóstico elegível";
    } else {
        plan.is_diagnostic_eligible = false;
        plan.diagnostic_eligibility_reason =
            "Mídia comercial ou não elegível: apenas binários sintéticos diagnósticos xblob são "
            "suportados para execução.";
    }

    // Process synthetic import thunks if a .thunk section is present
    for (const auto& sec : xbe.sections) {
        if (sec.name == ".thunk") {
            const std::size_t count = sec.raw_size / 8;
            std::vector<u8> thunk_data(sec.raw_size, 0);
            auto read_res = source.ReadAt(sec.raw_address, thunk_data);
            if (!read_res) {
                return read_res.error();
            }

            for (std::size_t i = 0; i < count; ++i) {
                u32 ord = static_cast<u32>(thunk_data[i * 8 + 0]) |
                          (static_cast<u32>(thunk_data[i * 8 + 1]) << 8) |
                          (static_cast<u32>(thunk_data[i * 8 + 2]) << 16) |
                          (static_cast<u32>(thunk_data[i * 8 + 3]) << 24);
                u32 thunk_offset = static_cast<u32>(thunk_data[i * 8 + 4]) |
                                   (static_cast<u32>(thunk_data[i * 8 + 5]) << 8) |
                                   (static_cast<u32>(thunk_data[i * 8 + 6]) << 16) |
                                   (static_cast<u32>(thunk_data[i * 8 + 7]) << 24);

                static const std::map<u32, std::string> kAllowlist = {
                    {10, "CreateThread"},     {11, "ExitThread"},   {12, "DbgPrint"},
                    {64, "CreateEvent"},      {65, "SetEvent"},     {66, "ResetEvent"},
                    {80, "CreateMutex"},      {84, "ReleaseMutex"}, {90, "WaitForSingleObject"},
                    {184, "RtlAllocateHeap"}, {185, "RtlFreeHeap"}};

                auto it = kAllowlist.find(ord);
                if (it == kAllowlist.end()) {
                    return Error{ErrorCode::UnsupportedFeature,
                                 "Ordinal de import não permitido ou não suportado", ord};
                }

                for (const auto& existing : plan.import_thunks) {
                    if (existing.ordinal == ord) {
                        return Error{ErrorCode::InvalidArgument,
                                     "Ordinal de import duplicado no plano", ord};
                    }
                }

                GuestAddr thunk_addr = sec.virtual_address + thunk_offset;
                plan.import_thunks.push_back(ImportThunkPlan{
                    .ordinal = ord,
                    .thunk_address = thunk_addr,
                    .name = it->second,
                });
            }
        }
    }

    return plan;
}

Result<void> XbeLoader::Apply(const XbeLoadPlan& plan, const ByteSource& source,
                              memory::AddressSpace& space) {
    std::vector<MemoryPatch> patches;

    // Helper to backup memory before writing
    auto backup_range = [&space, &patches](GuestAddr addr, GuestSize size) -> Result<void> {
        if (size == 0)
            return {};
        std::vector<u8> buf(size, 0);
        auto res = space.ReadBytes(addr, buf);
        if (!res) {
            return res.error();
        }
        patches.push_back(MemoryPatch{.addr = addr, .original = std::move(buf)});
        return {};
    };

    // 1. Backup headers range
    auto b_hdr = backup_range(plan.base_address, plan.headers_size);
    if (!b_hdr) {
        RollbackPatches(space, patches);
        return b_hdr.error();
    }

    // 2. Backup all sections ranges
    for (const auto& sec : plan.sections) {
        auto b_sec = backup_range(sec.virtual_address, sec.virtual_size);
        if (!b_sec) {
            RollbackPatches(space, patches);
            return b_sec.error();
        }
    }

    // 3. Write headers
    std::vector<u8> hdr_data(plan.headers_size, 0);
    auto read_hdr = source.ReadAt(0, hdr_data);
    if (!read_hdr) {
        RollbackPatches(space, patches);
        return Error{ErrorCode::IoError, "Falha ao ler cabeçalhos da fonte", 0};
    }

    auto write_hdr = space.WriteBytes(plan.base_address, hdr_data);
    if (!write_hdr) {
        RollbackPatches(space, patches);
        return write_hdr.error();
    }

    // 4. Write sections
    for (const auto& sec : plan.sections) {
        if (sec.raw_size > 0) {
            std::vector<u8> sec_data(sec.raw_size, 0);
            auto read_sec = source.ReadAt(sec.raw_address, sec_data);
            if (!read_sec) {
                RollbackPatches(space, patches);
                return Error{ErrorCode::IoError, "Falha ao ler dados da seção", sec.raw_address};
            }

            auto write_sec = space.WriteBytes(sec.virtual_address, sec_data);
            if (!write_sec) {
                RollbackPatches(space, patches);
                return write_sec.error();
            }
        }

        if (sec.zero_fill_size > 0) {
            const std::vector<u8> zeroes(sec.zero_fill_size, 0);
            auto write_zero = space.WriteBytes(sec.virtual_address + sec.raw_size, zeroes);
            if (!write_zero) {
                RollbackPatches(space, patches);
                return write_zero.error();
            }
        }
    }

    // 5. Materialize synthetic import thunks
    for (const auto& thunk : plan.import_thunks) {
        auto b_thunk = backup_range(thunk.thunk_address, 8);
        if (!b_thunk) {
            RollbackPatches(space, patches);
            return b_thunk.error();
        }

        // Trampoline: MOV EAX, ordinal (5 bytes: 0xB8, imm32); INT 0x2D (2 bytes: 0xCD, 0x2D); RET
        // (1 byte: 0xC3)
        std::vector<u8> code(8, 0);
        code[0] = 0xB8;
        code[1] = static_cast<u8>(thunk.ordinal & 0xFF);
        code[2] = static_cast<u8>((thunk.ordinal >> 8) & 0xFF);
        code[3] = static_cast<u8>((thunk.ordinal >> 16) & 0xFF);
        code[4] = static_cast<u8>((thunk.ordinal >> 24) & 0xFF);
        code[5] = 0xCD;
        code[6] = 0x2D;
        code[7] = 0xC3;

        auto w_thunk = space.WriteBytes(thunk.thunk_address, code);
        if (!w_thunk) {
            RollbackPatches(space, patches);
            return w_thunk.error();
        }
    }

    return {};
}

Result<XbeLoadPlan> XbeLoader::Load(const ByteSource& source, memory::AddressSpace& space) {
    auto plan = Plan(source);
    if (!plan) {
        return plan.error();
    }
    auto apply_res = Apply(*plan, source, space);
    if (!apply_res) {
        return apply_res.error();
    }
    return plan;
}

InitialContext XbeLoader::CreateInitialContext(const XbeLoadPlan& plan, GuestAddr stack_top,
                                               GuestSize stack_size) {
    InitialContext ctx;
    ctx.entry_point = plan.entry_point;
    ctx.base_address = plan.base_address;
    ctx.image_size = plan.image_size;
    ctx.stack_top = stack_top;
    ctx.stack_size = stack_size;

    ctx.cpu_context.eip = plan.entry_point;
    ctx.cpu_context.SetGpr(cpu::Reg32::ESP, stack_top);
    ctx.cpu_context.eflags = 0x00000002; // IA-32 bit 1 always set

    return ctx;
}

} // namespace xblob::loader
