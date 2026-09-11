#include "c_api_internal.hpp"
#include "xblob/c_api.h"
#include "xblob/formats/inspector.hpp"
#include "xblob/formats/xdvdfs.hpp"
#include "xblob/io/file_source.hpp"
#include "xblob/machine/machine_session.hpp"
#include "xblob/machine/media_boot_pipeline.hpp"
#include "xblob/vfs/vfs.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

struct xblob_vfs_browser_s {
    std::shared_ptr<const xblob::ByteSource> source;
    std::shared_ptr<xblob::XdvdfsVolume> volume;
};

using namespace xblob::c_api_internal;

extern "C" {

xblob_status_t xblob_machine_prepare_media(xblob_machine_t machine, const char* path_utf8,
                                           size_t path_length, xblob_boot_report_t* out_report) {
    if (!machine || !path_utf8) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (path_length == 0 || !is_valid_utf8(path_utf8, path_length)) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }
    if (out_report && out_report->struct_size != sizeof(xblob_boot_report_t)) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }

    try {
        std::filesystem::path fs_path(reinterpret_cast<const char8_t*>(path_utf8),
                                      reinterpret_cast<const char8_t*>(path_utf8 + path_length));
        auto file_res = xblob::FileSource::Open(fs_path);
        if (!file_res) {
            if (out_report) {
                out_report->is_bootable = 0;
                std::strncpy(out_report->error_message, file_res.error().message.c_str(),
                             sizeof(out_report->error_message) - 1);
            }
            return map_error_to_status(file_res.error().code);
        }

        std::shared_ptr<const xblob::ByteSource> source = std::move(*file_res);
        auto prep_res = machine->session->PrepareMedia(source);
        if (!prep_res) {
            if (out_report) {
                out_report->is_bootable = 0;
                std::strncpy(out_report->error_message, prep_res.error().message.c_str(),
                             sizeof(out_report->error_message) - 1);
            }
            return map_error_to_status(prep_res.error().code);
        }

        if (out_report) {
            std::memset(out_report, 0, sizeof(xblob_boot_report_t));
            out_report->struct_size = sizeof(xblob_boot_report_t);
            out_report->is_bootable = 1;
            out_report->media_size_bytes = source->size();

            // Detect media type
            if (source->size() >= 4) {
                xblob::u8 magic[4] = {0};
                (void)source->ReadAt(0, magic);
                if (magic[0] == 'X' && magic[1] == 'B' && magic[2] == 'E' && magic[3] == 'H') {
                    out_report->media_type = XBLOB_MEDIA_TYPE_XBE;
                    std::strncpy(out_report->default_xbe_path, "DEFAULT.XBE",
                                 sizeof(out_report->default_xbe_path) - 1);
                } else {
                    out_report->media_type = XBLOB_MEDIA_TYPE_XISO_TRIMMED;
                    std::strncpy(out_report->default_xbe_path, "D:\\DEFAULT.XBE",
                                 sizeof(out_report->default_xbe_path) - 1);
                }
            }

            auto vol_res = xblob::XdvdfsVolume::Open(source);
            if (vol_res) {
                auto xbe_file = (*vol_res)->OpenFile("DEFAULT.XBE");
                if (xbe_file) {
                    auto insp_res = xblob::MediaInspector::Inspect(**xbe_file);
                    if (insp_res && insp_res->xbe && insp_res->xbe->certificate) {
                        std::strncpy(out_report->title_name,
                                     insp_res->xbe->certificate->title_name.c_str(),
                                     sizeof(out_report->title_name) - 1);
                        out_report->title_id = insp_res->xbe->certificate->title_id;
                    }
                }
            } else {
                auto insp_res = xblob::MediaInspector::Inspect(*source);
                if (insp_res && insp_res->xbe && insp_res->xbe->certificate) {
                    std::strncpy(out_report->title_name,
                                 insp_res->xbe->certificate->title_name.c_str(),
                                 sizeof(out_report->title_name) - 1);
                    out_report->title_id = insp_res->xbe->certificate->title_id;
                }
            }

            const auto& plan = machine->session->load_plan();
            if (plan) {
                out_report->entry_point = plan->entry_point;
                out_report->section_count = static_cast<uint32_t>(plan->sections.size());
            }
        }

        return XBLOB_STATUS_OK;
    } catch (...) {
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

xblob_status_t xblob_vfs_browser_create(const char* path_utf8, size_t path_length,
                                        xblob_vfs_browser_t* out_browser) {
    if (!out_browser) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    *out_browser = nullptr;

    if (!path_utf8) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (path_length == 0 || !is_valid_utf8(path_utf8, path_length)) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }

    try {
        std::filesystem::path fs_path(reinterpret_cast<const char8_t*>(path_utf8),
                                      reinterpret_cast<const char8_t*>(path_utf8 + path_length));
        auto file_res = xblob::FileSource::Open(fs_path);
        if (!file_res) {
            return map_error_to_status(file_res.error().code);
        }

        std::shared_ptr<const xblob::ByteSource> source = std::move(*file_res);
        auto vol_res = xblob::XdvdfsVolume::Open(source);
        if (!vol_res) {
            return map_error_to_status(vol_res.error().code);
        }

        auto* browser = new (std::nothrow) xblob_vfs_browser_s();
        if (!browser) {
            return XBLOB_STATUS_ERROR_INTERNAL;
        }

        browser->source = std::move(source);
        browser->volume = std::move(*vol_res);
        *out_browser = browser;
        return XBLOB_STATUS_OK;
    } catch (...) {
        *out_browser = nullptr;
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
}

void xblob_vfs_browser_destroy(xblob_vfs_browser_t browser) {
    delete browser;
}

xblob_status_t xblob_vfs_browser_get_entry_count(xblob_vfs_browser_t browser,
                                                 const char* dir_path_utf8, size_t dir_path_len,
                                                 uint32_t* out_count) {
    if (!browser || !out_count) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    std::string rel_path;
    if (dir_path_utf8 && dir_path_len > 0) {
        if (!is_valid_utf8(dir_path_utf8, dir_path_len)) {
            return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
        }
        rel_path.assign(dir_path_utf8, dir_path_len);
    }

    auto list_res = browser->volume->ListDirectory(rel_path);
    if (!list_res) {
        return map_error_to_status(list_res.error().code);
    }

    *out_count = static_cast<uint32_t>(list_res->size());
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_vfs_browser_list_entries(xblob_vfs_browser_t browser,
                                              const char* dir_path_utf8, size_t dir_path_len,
                                              uint32_t offset, uint32_t limit,
                                              xblob_dir_entry_t* out_entries,
                                              uint32_t* inout_count) {
    if (!browser || !inout_count) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }

    std::string rel_path;
    if (dir_path_utf8 && dir_path_len > 0) {
        if (!is_valid_utf8(dir_path_utf8, dir_path_len)) {
            return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
        }
        rel_path.assign(dir_path_utf8, dir_path_len);
    }

    auto list_res = browser->volume->ListDirectory(rel_path);
    if (!list_res) {
        return map_error_to_status(list_res.error().code);
    }

    const auto& all = *list_res;
    if (offset >= all.size()) {
        *inout_count = 0;
        return XBLOB_STATUS_OK;
    }

    const uint32_t available = static_cast<uint32_t>(all.size() - offset);
    const uint32_t count_to_copy = std::min(available, limit);

    if (!out_entries) {
        // First call in two-call pattern: return count that would be copied
        *inout_count = count_to_copy;
        return XBLOB_STATUS_OK;
    }

    if (*inout_count < count_to_copy) {
        *inout_count = count_to_copy;
        return XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL;
    }

    for (uint32_t i = 0; i < count_to_copy; ++i) {
        const auto& entry = all[offset + i];
        xblob_dir_entry_t& dst = out_entries[i];
        std::memset(&dst, 0, sizeof(xblob_dir_entry_t));
        std::strncpy(dst.name, entry.name.c_str(), sizeof(dst.name) - 1);
        dst.size = entry.file_size;
        dst.is_directory = entry.is_directory ? 1 : 0;
        dst.attributes = entry.attributes;
    }

    *inout_count = count_to_copy;
    return XBLOB_STATUS_OK;
}

} // extern "C"
