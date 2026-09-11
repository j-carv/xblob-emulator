#pragma once

#include <ostream>
#include <string_view>

namespace xblob {

enum class MediaType { Unknown = 0, XbeExecutable, XisoTrimmed, XisoRaw, Iso9660Unsupported };

[[nodiscard]] constexpr std::string_view MediaTypeToString(MediaType type) noexcept {
    switch (type) {
    case MediaType::XbeExecutable:
        return "Executável Xbox (XBE)";
    case MediaType::XisoTrimmed:
        return "Imagem de Disco Xbox (XISO Otimizada/Trimmed)";
    case MediaType::XisoRaw:
        return "Imagem de Disco Xbox (XISO Raw/Redump)";
    case MediaType::Iso9660Unsupported:
        return "Imagem ISO 9660 Padrão (Sem partição Xbox)";
    case MediaType::Unknown:
    default:
        return "Formato Desconhecido";
    }
}

inline std::ostream& operator<<(std::ostream& os, MediaType type) {
    return os << MediaTypeToString(type);
}

} // namespace xblob
