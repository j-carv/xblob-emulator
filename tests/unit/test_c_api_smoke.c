#include "xblob/c_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint32_t major = xblob_get_abi_version_major();
    assert(major == 1);

    uint32_t minor = xblob_get_abi_version_minor();
    assert(minor == 4);

    uint32_t patch = xblob_get_abi_version_patch();
    assert(patch == 0);

    uint64_t caps = xblob_get_capabilities();
    assert((caps & XBLOB_CAPABILITY_MEDIA_INSPECTION) != 0);
    assert((caps & XBLOB_CAPABILITY_MACHINE_SESSION) != 0);
    assert((caps & XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION) != 0);
    assert((caps & XBLOB_CAPABILITY_FRAMEBUFFER_PRESENTATION) != 0);
    assert((caps & XBLOB_CAPABILITY_NV2A_GPU) != 0);
    assert((caps & XBLOB_CAPABILITY_XDVDFS_VFS) != 0);
    assert((caps & XBLOB_CAPABILITY_MEDIA_BOOT) != 0);

    const char* version = xblob_get_product_version();
    assert(version != NULL);
    assert(strcmp(version, "0.1.0") == 0);

    const char* name = xblob_get_product_name();
    assert(name != NULL);
    assert(strcmp(name, "xblob") == 0);

    const char* ok_str = xblob_status_to_string(XBLOB_STATUS_OK);
    assert(ok_str != NULL);
    assert(strcmp(ok_str, "OK") == 0);

    xblob_core_info_t info;
    memset(&info, 0, sizeof(info));
    info.struct_size = (uint32_t)sizeof(xblob_core_info_t);
    xblob_status_t status = xblob_get_core_info(&info);
    assert(status == XBLOB_STATUS_OK);
    assert(info.abi_version_major == 1);
    assert(info.abi_version_minor == 4);
    assert(info.capabilities == caps);
    assert(strcmp(info.product_name, "xblob") == 0);

    // Verify null destroy and null call safety
    xblob_media_report_destroy(NULL);
    xblob_machine_destroy(NULL);
    xblob_vfs_browser_destroy(NULL);
    assert(xblob_machine_step(NULL, 1, NULL) == XBLOB_STATUS_ERROR_NULL_POINTER);
    assert(xblob_machine_prepare_media(NULL, NULL, 0, NULL) == XBLOB_STATUS_ERROR_NULL_POINTER);
    assert(xblob_vfs_browser_create(NULL, 0, NULL) == XBLOB_STATUS_ERROR_NULL_POINTER);

    // Frame presentation API null safety
    xblob_frame_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.struct_size = (uint32_t)sizeof(meta);
    assert(xblob_machine_get_frame_metadata(NULL, &meta) == XBLOB_STATUS_ERROR_NULL_POINTER);
    assert(xblob_machine_copy_frame_pixels(NULL, NULL, NULL) == XBLOB_STATUS_ERROR_NULL_POINTER);

    printf("C11 smoke test passed successfully.\n");
    return 0;
}
