#include "xblob/formats/inspector.hpp"

#include "xblob/io/file_source.hpp"

#include <iomanip>
#include <sstream>

namespace xblob {

std::string MediaReport::FormatHumanReadable() const {
    std::ostringstream oss;
    oss << "========================================================\n";
    oss << " xblob Relatório de Inspeção de Mídia\n";
    oss << "========================================================\n";
    if (!file_path.empty()) {
        oss << "Arquivo:             " << file_path << "\n";
    }
    oss << "Tamanho:             " << file_size << " bytes\n";
    oss << "Formato detectado:   " << MediaTypeToString(type) << "\n";

    if (xbe) {
        oss << "\n--- Metadados do Cabeçalho XBE ---\n";
        oss << "Endereço base:       0x" << std::hex << std::setw(8) << std::setfill('0')
            << xbe->header.base_address << std::dec << "\n";
        oss << "Tamanho cabeçalhos:  0x" << std::hex << xbe->header.headers_size << " (" << std::dec
            << xbe->header.headers_size << " bytes)\n";
        oss << "Tamanho da imagem:   0x" << std::hex << xbe->header.image_size << " (" << std::dec
            << xbe->header.image_size << " bytes)\n";
        oss << "Timestamp:           0x" << std::hex << xbe->header.timestamp << std::dec << "\n";
        oss << "Ponto de entrada:    0x" << std::hex << std::setw(8) << std::setfill('0')
            << xbe->header.entry_point << std::dec << "\n";
        oss << "Total de seções:     " << xbe->sections.size() << "\n";

        if (xbe->certificate) {
            oss << "\n--- Certificado do Título ---\n";
            oss << "Title ID:            0x" << std::hex << std::setw(8) << std::setfill('0')
                << xbe->certificate->title_id << std::dec << "\n";
            oss << "Nome do título:      \"" << xbe->certificate->title_name << "\"\n";
            oss << "Versão:              " << xbe->certificate->version << "\n";
            oss << "Região do jogo:      0x" << std::hex << xbe->certificate->game_region
                << std::dec << "\n";
            oss << "Número do disco:     " << xbe->certificate->disk_number << "\n";
        }

        if (!xbe->sections.empty()) {
            oss << "\n--- Seções ---\n";
            for (size_t i = 0; i < xbe->sections.size(); ++i) {
                const auto& sec = xbe->sections[i];
                oss << " [" << i << "] " << (sec.name.empty() ? "(sem nome)" : sec.name)
                    << " | VAddr: 0x" << std::hex << std::setw(8) << std::setfill('0')
                    << sec.virtual_address << " (tam: 0x" << sec.virtual_size << ")"
                    << " | Raw: 0x" << sec.raw_address << " (tam: 0x" << sec.raw_size << ")"
                    << std::dec << "\n";
            }
        }
    }

    if (xiso) {
        oss << "\n--- Metadados do Sistema de Arquivos XDVDFS ---\n";
        oss << "Variante:            " << MediaTypeToString(xiso->variant) << "\n";
        oss << "Offset do Descritor: 0x" << std::hex << xiso->volume_descriptor_offset << " ("
            << std::dec << xiso->volume_descriptor_offset << " bytes)\n";
        oss << "Setor raiz dir:      " << xiso->root_dir_sector << " (offset 0x" << std::hex
            << (static_cast<u64>(xiso->root_dir_sector) * 2048ULL) << std::dec << ")\n";
        oss << "Tamanho raiz dir:    " << xiso->root_dir_size << " bytes\n";
        oss << "Assinatura rodapé:   "
            << (xiso->valid_footer_magic ? "Válida (MICROSOFT*XBOX*MEDIA)" : "Ausente / Inválida")
            << "\n";
    }

    oss << "========================================================\n";
    return oss.str();
}

Result<MediaReport> MediaInspector::Inspect(const ByteSource& source, std::string_view file_path) {
    // 1. Check for XBE signature
    auto is_xbe_res = XbeParser::IsXbe(source);
    if (is_xbe_res && *is_xbe_res) {
        auto parse_res = XbeParser::Parse(source);
        if (!parse_res) {
            return parse_res.error();
        }
        MediaReport report{};
        report.type = MediaType::XbeExecutable;
        report.file_size = source.size();
        report.file_path = std::string(file_path);
        report.xbe = std::move(*parse_res);
        return report;
    }

    // 2. Check for XISO / ISO detection
    auto detect_res = XisoDetector::Detect(source);
    if (detect_res) {
        MediaType media = *detect_res;
        if (media == MediaType::XisoTrimmed || media == MediaType::XisoRaw) {
            auto inspect_iso_res = XisoDetector::Inspect(source);
            if (!inspect_iso_res) {
                return inspect_iso_res.error();
            }
            MediaReport report{};
            report.type = media;
            report.file_size = source.size();
            report.file_path = std::string(file_path);
            report.xiso = std::move(*inspect_iso_res);
            return report;
        }

        if (media == MediaType::Iso9660Unsupported) {
            return Error{
                ErrorCode::UnsupportedFormat,
                "Imagem ISO 9660 padrão detectada; contêiner não possui partição Xbox XDVDFS",
                0x8000};
        }
    }

    return Error{ErrorCode::UnknownFormat,
                 "O formato do arquivo não pôde ser identificado como um executável ou mídia de "
                 "disco Xbox suportado",
                 0};
}

Result<MediaReport> MediaInspector::InspectFile(const std::filesystem::path& path) {
    auto file_res = FileSource::Open(path);
    if (!file_res) {
        return file_res.error();
    }
    return Inspect(**file_res, path.string());
}

} // namespace xblob
