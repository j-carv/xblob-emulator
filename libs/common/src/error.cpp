#include "xblob/common/error.hpp"

namespace xblob {

std::string_view ErrorCodeToString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Ok:
        return "Ok";
    case ErrorCode::FileNotFound:
        return "Arquivo não encontrado";
    case ErrorCode::AccessDenied:
        return "Acesso negado";
    case ErrorCode::IoError:
        return "Erro de E/S";
    case ErrorCode::UnexpectedEof:
        return "Fim prematuro de arquivo";
    case ErrorCode::InvalidPath:
        return "Caminho inválido ou tentativa de traversal";
    case ErrorCode::NotADirectory:
        return "Componente de caminho não é um diretório";
    case ErrorCode::IsADirectory:
        return "Alvo é um diretório e não um arquivo";
    case ErrorCode::InvalidHandle:
        return "Handle de arquivo/diretório inválido ou stale";
    case ErrorCode::ReadOnlyFileSystem:
        return "Sistema de arquivos montado como somente leitura";
    case ErrorCode::InvalidMagic:
        return "Assinatura mágica inválida";
    case ErrorCode::UnknownFormat:
        return "Formato desconhecido";
    case ErrorCode::TruncatedData:
        return "Dados truncados";
    case ErrorCode::IntegerOverflow:
        return "Overflow aritmético";
    case ErrorCode::OutOfBounds:
        return "Acesso fora dos limites";
    case ErrorCode::InvalidHeader:
        return "Cabeçalho inválido";
    case ErrorCode::InvalidField:
        return "Campo inválido";
    case ErrorCode::UnsupportedFormat:
        return "Formato ou variante não suportada";
    case ErrorCode::UnsupportedFeature:
        return "Recurso ainda não implementado";
    case ErrorCode::UnmappedAddress:
        return "Endereço não mapeado";
    case ErrorCode::AccessViolation:
        return "Violação de permissão de acesso";
    case ErrorCode::RegionOverlap:
        return "Sobreposição de regiões de memória";
    case ErrorCode::InvalidCapacity:
        return "Capacidade de memória inválida";
    case ErrorCode::UnsupportedAccessSize:
        return "Tamanho de acesso não suportado";
    case ErrorCode::PastCycle:
        return "Ciclo alvo no passado";
    case ErrorCode::EventNotFound:
        return "Evento não encontrado";
    case ErrorCode::LimitReached:
        return "Limite de execução atingido";
    case ErrorCode::InvalidOpcode:
        return "Opcode inválido";
    case ErrorCode::GeneralProtection:
        return "Falha geral de proteção (#GP)";
    case ErrorCode::SegmentNotPresent:
        return "Segmento ou gate não presente (#NP)";
    case ErrorCode::ExecutionFault:
        return "Falha na execução da CPU";
    case ErrorCode::CpuHalted:
        return "CPU em estado de parada (HLT)";
    case ErrorCode::InvalidArgument:
        return "Argumento inválido";
    case ErrorCode::InvalidState:
        return "Estado de sessão ou máquina inválido";
    case ErrorCode::InternalError:
        return "Erro interno";
    }
    return "Erro desconhecido";
}

} // namespace xblob
