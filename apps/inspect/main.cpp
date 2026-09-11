#include "xblob/common/error.hpp"
#include "xblob/formats/inspector.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void PrintUsage(std::string_view program_name) {
    std::cout << "Uso: " << program_name << " [opções] <caminho-do-arquivo>\n\n"
              << "Inspeciona arquivos executáveis Xbox (XBE) e imagens de disco (ISO/XISO) de\n"
              << "forma segura e defensiva, sem executar código convidado.\n\n"
              << "Opções:\n"
              << "  -h, --help       Exibe esta mensagem de ajuda e sai\n"
              << "  -v, --version    Exibe informações de versão do xblob e sai\n\n"
              << "Códigos de Saída:\n"
              << "  0  Sucesso (mídia inspecionada e válida)\n"
              << "  1  Argumentos ausentes ou opções inválidas\n"
              << "  2  Erro de E/S ou arquivo não encontrado\n"
              << "  3  Formato corrompido, truncado ou inválido\n"
              << "  4  Formato reconhecido, porém variante não suportada\n";
}

void PrintVersion() {
    std::cout << "xblob-inspect v0.1.0 (Marco 1: Bootstrap)\n"
              << "Emulador Xbox Clássico - Inspeção Segura de Mídia\n";
}

int MapErrorToExitCode(xblob::ErrorCode code) {
    switch (code) {
    case xblob::ErrorCode::Ok:
        return 0;

    case xblob::ErrorCode::InvalidArgument:
        return 1;

    case xblob::ErrorCode::FileNotFound:
    case xblob::ErrorCode::AccessDenied:
    case xblob::ErrorCode::IoError:
    case xblob::ErrorCode::UnexpectedEof:
    case xblob::ErrorCode::InvalidPath:
    case xblob::ErrorCode::NotADirectory:
    case xblob::ErrorCode::IsADirectory:
    case xblob::ErrorCode::InvalidHandle:
    case xblob::ErrorCode::ReadOnlyFileSystem:
        return 2;

    case xblob::ErrorCode::InvalidMagic:
    case xblob::ErrorCode::UnknownFormat:
    case xblob::ErrorCode::TruncatedData:
    case xblob::ErrorCode::IntegerOverflow:
    case xblob::ErrorCode::OutOfBounds:
    case xblob::ErrorCode::InvalidHeader:
    case xblob::ErrorCode::InvalidField:
    case xblob::ErrorCode::InternalError:
    case xblob::ErrorCode::UnmappedAddress:
    case xblob::ErrorCode::AccessViolation:
    case xblob::ErrorCode::RegionOverlap:
    case xblob::ErrorCode::InvalidCapacity:
    case xblob::ErrorCode::UnsupportedAccessSize:
    case xblob::ErrorCode::PastCycle:
    case xblob::ErrorCode::EventNotFound:
    case xblob::ErrorCode::LimitReached:
    case xblob::ErrorCode::InvalidOpcode:
    case xblob::ErrorCode::GeneralProtection:
    case xblob::ErrorCode::SegmentNotPresent:
    case xblob::ErrorCode::ExecutionFault:
    case xblob::ErrorCode::CpuHalted:
    case xblob::ErrorCode::InvalidState:
        return 3;

    case xblob::ErrorCode::UnsupportedFormat:
    case xblob::ErrorCode::UnsupportedFeature:
        return 4;
    }
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Erro: Nenhum arquivo especificado para inspeção.\n\n";
        PrintUsage(argv[0]);
        return 1;
    }

    std::string_view arg1 = argv[1];
    if (arg1 == "-h" || arg1 == "--help") {
        PrintUsage(argv[0]);
        return 0;
    }
    if (arg1 == "-v" || arg1 == "--version") {
        PrintVersion();
        return 0;
    }

    std::filesystem::path target_path(arg1);
    auto report_res = xblob::MediaInspector::InspectFile(target_path);

    if (!report_res) {
        const auto& err = report_res.error();
        std::cerr << "Erro [" << xblob::ErrorCodeToString(err.code) << "]: " << err.message;
        if (err.offset > 0) {
            std::cerr << " (offset 0x" << std::hex << err.offset << std::dec << ")";
        }
        std::cerr << "\n";
        return MapErrorToExitCode(err.code);
    }

    std::cout << report_res->FormatHumanReadable();
    return 0;
}
