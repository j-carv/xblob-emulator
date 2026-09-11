#include "xblob/c_api.h"

#include "c_api_internal.hpp"
#include "xblob/common/error.hpp"
#include "xblob/formats/inspector.hpp"
#include "xblob/formats/media_type.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <string_view>

struct xblob_media_report_s {
    xblob::MediaReport report{};
    std::string error_message{};
};

namespace xblob::c_api_internal {

bool is_valid_utf8(const char* data, size_t length) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    size_t i = 0;
    while (i < length) {
        if (bytes[i] <= 0x7F) {
            i += 1;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            if (i + 1 >= length || (bytes[i + 1] & 0xC0) != 0x80)
                return false;
            if (bytes[i] < 0xC2)
                return false; // Overlong
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            if (i + 2 >= length || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80)
                return false;
            if (bytes[i] == 0xE0 && bytes[i + 1] < 0xA0)
                return false; // Overlong
            if (bytes[i] == 0xED && bytes[i + 1] >= 0xA0)
                return false; // Surrogate
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            if (i + 3 >= length || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 ||
                (bytes[i + 3] & 0xC0) != 0x80)
                return false;
            if (bytes[i] == 0xF0 && bytes[i + 1] < 0x90)
                return false; // Overlong
            if (bytes[i] == 0xF4 && bytes[i + 1] > 0x8F)
                return false; // Above U+10FFFF
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

xblob_status_t map_error_to_status(xblob::ErrorCode code) noexcept {
    switch (code) {
    case xblob::ErrorCode::Ok:
        return XBLOB_STATUS_OK;
    case xblob::ErrorCode::FileNotFound:
        return XBLOB_STATUS_ERROR_NOT_FOUND;
    case xblob::ErrorCode::AccessDenied:
    case xblob::ErrorCode::IoError:
    case xblob::ErrorCode::UnexpectedEof:
        return XBLOB_STATUS_ERROR_IO;
    case xblob::ErrorCode::UnknownFormat:
    case xblob::ErrorCode::UnsupportedFormat:
    case xblob::ErrorCode::UnsupportedFeature:
        return XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT;
    case xblob::ErrorCode::InvalidMagic:
    case xblob::ErrorCode::TruncatedData:
    case xblob::ErrorCode::InvalidHeader:
    case xblob::ErrorCode::InvalidField:
    case xblob::ErrorCode::OutOfBounds:
    case xblob::ErrorCode::IntegerOverflow:
        return XBLOB_STATUS_ERROR_CORRUPT_MEDIA;
    case xblob::ErrorCode::InvalidArgument:
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    case xblob::ErrorCode::InvalidState:
        return XBLOB_STATUS_ERROR_INVALID_STATE;
    default:
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

xblob_media_type_t map_media_type(xblob::MediaType type) noexcept {
    switch (type) {
    case xblob::MediaType::XbeExecutable:
        return XBLOB_MEDIA_TYPE_XBE;
    case xblob::MediaType::XisoTrimmed:
        return XBLOB_MEDIA_TYPE_XISO_TRIMMED;
    case xblob::MediaType::XisoRaw:
        return XBLOB_MEDIA_TYPE_XISO_RAW;
    case xblob::MediaType::Iso9660Unsupported:
        return XBLOB_MEDIA_TYPE_ISO9660_UNSUPPORTED;
    case xblob::MediaType::Unknown:
    default:
        return XBLOB_MEDIA_TYPE_UNKNOWN;
    }
}

xblob_status_t copy_string_to_two_call_buffer(std::string_view str, char* buffer,
                                              size_t* inout_buffer_size) noexcept {
    if (!inout_buffer_size) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    const size_t required_size = str.size() + 1;
    if (!buffer) {
        *inout_buffer_size = required_size;
        return XBLOB_STATUS_OK;
    }
    if (*inout_buffer_size < required_size) {
        *inout_buffer_size = required_size;
        return XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buffer, str.data(), str.size());
    buffer[str.size()] = '\0';
    *inout_buffer_size = required_size;
    return XBLOB_STATUS_OK;
}

} // namespace xblob::c_api_internal

using namespace xblob::c_api_internal;

extern "C" {

uint32_t xblob_get_abi_version_major(void) {
    return XBLOB_C_API_VERSION_MAJOR;
}

uint32_t xblob_get_abi_version_minor(void) {
    return XBLOB_C_API_VERSION_MINOR;
}

uint32_t xblob_get_abi_version_patch(void) {
    return XBLOB_C_API_VERSION_PATCH;
}

uint64_t xblob_get_capabilities(void) {
    return XBLOB_CAPABILITY_MEDIA_INSPECTION | XBLOB_CAPABILITY_DETERMINISTIC_SCHEDULER |
           XBLOB_CAPABILITY_GUEST_MEMORY | XBLOB_CAPABILITY_SYNTHETIC_CPU |
           XBLOB_CAPABILITY_GUEST_BUS | XBLOB_CAPABILITY_VIRTUAL_MEMORY |
           XBLOB_CAPABILITY_XBE_LOADER | XBLOB_CAPABILITY_MACHINE_SESSION |
           XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION;
}

const char* xblob_get_product_version(void) {
    return "0.1.0";
}

const char* xblob_get_product_name(void) {
    return "xblob";
}

const char* xblob_status_to_string(xblob_status_t status) {
    switch (status) {
    case XBLOB_STATUS_OK:
        return "OK";
    case XBLOB_STATUS_ERROR_INVALID_ARGUMENT:
        return "Argumento invalido";
    case XBLOB_STATUS_ERROR_NULL_POINTER:
        return "Ponteiro nulo";
    case XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL:
        return "Buffer insuficiente";
    case XBLOB_STATUS_ERROR_NOT_FOUND:
        return "Arquivo ou recurso nao encontrado";
    case XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT:
        return "Formato nao suportado";
    case XBLOB_STATUS_ERROR_CORRUPT_MEDIA:
        return "Midia ou cabecalho corrompido";
    case XBLOB_STATUS_ERROR_IO:
        return "Erro de I/O";
    case XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION:
        return "Versao ou estrutura incompativel";
    case XBLOB_STATUS_ERROR_INVALID_STATE:
        return "Estado de operacao invalido";
    case XBLOB_STATUS_ERROR_INTERNAL:
        return "Erro interno";
    default:
        return "Status desconhecido";
    }
}

xblob_status_t xblob_get_core_info(xblob_core_info_t* out_info) {
    if (!out_info) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_info->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }

    try {
        xblob_core_info_t full{};
        full.struct_size = sizeof(xblob_core_info_t);
        full.abi_version_major = XBLOB_C_API_VERSION_MAJOR;
        full.abi_version_minor = XBLOB_C_API_VERSION_MINOR;
        full.abi_version_patch = XBLOB_C_API_VERSION_PATCH;
        full.capabilities = xblob_get_capabilities();
        std::strncpy(full.product_name, xblob_get_product_name(), sizeof(full.product_name) - 1);
        std::strncpy(full.product_version, xblob_get_product_version(),
                     sizeof(full.product_version) - 1);

        const size_t copy_size =
            std::min(static_cast<size_t>(out_info->struct_size), sizeof(xblob_core_info_t));
        std::memcpy(out_info, &full, copy_size);
        return XBLOB_STATUS_OK;
    } catch (...) {
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

xblob_status_t xblob_media_inspect(const char* path_utf8, size_t path_length,
                                   xblob_media_report_t* out_report) {
    if (!out_report) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    *out_report = nullptr;

    if (!path_utf8) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (path_length == 0) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }
    if (!is_valid_utf8(path_utf8, path_length)) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }

    try {
        std::filesystem::path fs_path(reinterpret_cast<const char8_t*>(path_utf8),
                                      reinterpret_cast<const char8_t*>(path_utf8 + path_length));

        auto inspect_res = xblob::MediaInspector::InspectFile(fs_path);
        if (!inspect_res) {
            return map_error_to_status(inspect_res.error().code);
        }

        auto* rep = new (std::nothrow) xblob_media_report_s();
        if (!rep) {
            return XBLOB_STATUS_ERROR_INTERNAL;
        }
        rep->report = std::move(*inspect_res);
        *out_report = rep;
        return XBLOB_STATUS_OK;
    } catch (...) {
        *out_report = nullptr;
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

void xblob_media_report_destroy(xblob_media_report_t report) {
    delete report;
}

xblob_status_t xblob_media_report_get_type(const struct xblob_media_report_s* report,
                                           xblob_media_type_t* out_type) {
    if (!report || !out_type) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    *out_type = map_media_type(report->report.type);
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_file_size(const struct xblob_media_report_s* report,
                                                uint64_t* out_size) {
    if (!report || !out_size) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    *out_size = report->report.file_size;
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_file_path(const struct xblob_media_report_s* report,
                                                char* buffer, size_t* inout_buffer_size) {
    if (!report) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    return copy_string_to_two_call_buffer(report->report.file_path, buffer, inout_buffer_size);
}

xblob_status_t xblob_media_report_get_title(const struct xblob_media_report_s* report, char* buffer,
                                            size_t* inout_buffer_size) {
    if (!report) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    std::string_view title;
    if (report->report.xbe && report->report.xbe->certificate) {
        title = report->report.xbe->certificate->title_name;
    }
    return copy_string_to_two_call_buffer(title, buffer, inout_buffer_size);
}

xblob_status_t xblob_media_report_get_human_summary(const struct xblob_media_report_s* report,
                                                    char* buffer, size_t* inout_buffer_size) {
    if (!report) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    try {
        std::string summary = report->report.FormatHumanReadable();
        return copy_string_to_two_call_buffer(summary, buffer, inout_buffer_size);
    } catch (...) {
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

xblob_status_t xblob_media_report_get_title_id(const struct xblob_media_report_s* report,
                                               uint32_t* out_title_id, int* out_has_title_id) {
    if (!report || !out_title_id || !out_has_title_id) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (report->report.xbe && report->report.xbe->certificate) {
        *out_title_id = report->report.xbe->certificate->title_id;
        *out_has_title_id = 1;
    } else {
        *out_title_id = 0;
        *out_has_title_id = 0;
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_disk_number(const struct xblob_media_report_s* report,
                                                  uint32_t* out_disk_number,
                                                  int* out_has_disk_number) {
    if (!report || !out_disk_number || !out_has_disk_number) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (report->report.xbe && report->report.xbe->certificate) {
        *out_disk_number = report->report.xbe->certificate->disk_number;
        *out_has_disk_number = 1;
    } else {
        *out_disk_number = 0;
        *out_has_disk_number = 0;
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_game_region(const struct xblob_media_report_s* report,
                                                  uint32_t* out_game_region,
                                                  int* out_has_game_region) {
    if (!report || !out_game_region || !out_has_game_region) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (report->report.xbe && report->report.xbe->certificate) {
        *out_game_region = report->report.xbe->certificate->game_region;
        *out_game_region = report->report.xbe->certificate->game_region;
        *out_has_game_region = 1;
    } else {
        *out_game_region = 0;
        *out_has_game_region = 0;
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_entry_point(const struct xblob_media_report_s* report,
                                                  uint32_t* out_entry_point,
                                                  int* out_has_entry_point) {
    if (!report || !out_entry_point || !out_has_entry_point) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (report->report.xbe) {
        *out_entry_point = report->report.xbe->header.entry_point;
        *out_has_entry_point = 1;
    } else {
        *out_entry_point = 0;
        *out_has_entry_point = 0;
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_section_count(const struct xblob_media_report_s* report,
                                                    uint32_t* out_section_count) {
    if (!report || !out_section_count) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (report->report.xbe) {
        *out_section_count = static_cast<uint32_t>(report->report.xbe->sections.size());
    } else {
        *out_section_count = 0;
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_media_report_get_error_message(const struct xblob_media_report_s* report,
                                                    char* buffer, size_t* inout_buffer_size) {
    if (!report) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    return xblob::c_api_internal::copy_string_to_two_call_buffer(report->error_message, buffer,
                                                                 inout_buffer_size);
}

} // extern "C"
