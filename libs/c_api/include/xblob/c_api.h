#ifndef XBLOB_C_API_H
#define XBLOB_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(XBLOB_C_API_BUILD)
#define XBLOB_C_API_EXPORT __declspec(dllexport)
#else
#define XBLOB_C_API_EXPORT __declspec(dllimport)
#endif
#define XBLOB_C_API_CALL __cdecl
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define XBLOB_C_API_EXPORT __attribute__((visibility("default")))
#else
#define XBLOB_C_API_EXPORT
#endif
#define XBLOB_C_API_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XBLOB_C_API_VERSION_MAJOR 1
#define XBLOB_C_API_VERSION_MINOR 6
#define XBLOB_C_API_VERSION_PATCH 0

typedef enum xblob_status_t {
    XBLOB_STATUS_OK = 0,
    XBLOB_STATUS_ERROR_INVALID_ARGUMENT = 1,
    XBLOB_STATUS_ERROR_NULL_POINTER = 2,
    XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL = 3,
    XBLOB_STATUS_ERROR_NOT_FOUND = 4,
    XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT = 5,
    XBLOB_STATUS_ERROR_CORRUPT_MEDIA = 6,
    XBLOB_STATUS_ERROR_IO = 7,
    XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION = 8,
    XBLOB_STATUS_ERROR_INVALID_STATE = 9,
    XBLOB_STATUS_ERROR_INTERNAL = 99
} xblob_status_t;

#define XBLOB_CAPABILITY_NONE 0ULL
#define XBLOB_CAPABILITY_MEDIA_INSPECTION (1ULL << 0)
#define XBLOB_CAPABILITY_DETERMINISTIC_SCHEDULER (1ULL << 1)
#define XBLOB_CAPABILITY_GUEST_MEMORY (1ULL << 2)
#define XBLOB_CAPABILITY_SYNTHETIC_CPU (1ULL << 3)
#define XBLOB_CAPABILITY_GUEST_BUS (1ULL << 4)
#define XBLOB_CAPABILITY_VIRTUAL_MEMORY (1ULL << 5)
#define XBLOB_CAPABILITY_XBE_LOADER (1ULL << 6)
#define XBLOB_CAPABILITY_MACHINE_SESSION (1ULL << 7)
#define XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION (1ULL << 8)
#define XBLOB_CAPABILITY_FRAMEBUFFER_PRESENTATION (1ULL << 9)
#define XBLOB_CAPABILITY_NV2A_GPU (1ULL << 10)
#define XBLOB_CAPABILITY_XDVDFS_VFS (1ULL << 11)
#define XBLOB_CAPABILITY_MEDIA_BOOT (1ULL << 12)
#define XBLOB_CAPABILITY_EXPERIMENTAL_TITLE_EXECUTION (1ULL << 13)
#define XBLOB_CAPABILITY_COMPATIBILITY_DIAGNOSTICS (1ULL << 14)
#define XBLOB_CAPABILITY_NV2A_3D (1ULL << 15)
#define XBLOB_CAPABILITY_USB_OHCI (1ULL << 16)
#define XBLOB_CAPABILITY_XID_INPUT (1ULL << 17)
#define XBLOB_CAPABILITY_INTERACTIVE_SESSION (1ULL << 18)

typedef enum xblob_media_type_t {
    XBLOB_MEDIA_TYPE_UNKNOWN = 0,
    XBLOB_MEDIA_TYPE_XBE = 1,
    XBLOB_MEDIA_TYPE_XISO_TRIMMED = 2,
    XBLOB_MEDIA_TYPE_XISO_RAW = 3,
    XBLOB_MEDIA_TYPE_ISO9660_UNSUPPORTED = 4
} xblob_media_type_t;

typedef struct xblob_core_info_t {
    uint32_t struct_size;
    uint32_t abi_version_major;
    uint32_t abi_version_minor;
    uint32_t abi_version_patch;
    uint64_t capabilities;
    char product_name[64];
    char product_version[32];
} xblob_core_info_t;

struct xblob_media_report_s;
typedef struct xblob_media_report_s* xblob_media_report_t;

XBLOB_C_API_EXPORT uint32_t XBLOB_C_API_CALL xblob_get_abi_version_major(void);
XBLOB_C_API_EXPORT uint32_t XBLOB_C_API_CALL xblob_get_abi_version_minor(void);
XBLOB_C_API_EXPORT uint32_t XBLOB_C_API_CALL xblob_get_abi_version_patch(void);
XBLOB_C_API_EXPORT uint64_t XBLOB_C_API_CALL xblob_get_capabilities(void);
XBLOB_C_API_EXPORT const char* XBLOB_C_API_CALL xblob_get_product_version(void);
XBLOB_C_API_EXPORT const char* XBLOB_C_API_CALL xblob_get_product_name(void);
XBLOB_C_API_EXPORT const char* XBLOB_C_API_CALL xblob_status_to_string(xblob_status_t status);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_get_core_info(xblob_core_info_t* out_info);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_media_inspect(const char* path_utf8, size_t path_length, xblob_media_report_t* out_report);

XBLOB_C_API_EXPORT void XBLOB_C_API_CALL xblob_media_report_destroy(xblob_media_report_t report);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_type(
    const struct xblob_media_report_s* report, xblob_media_type_t* out_type);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_media_report_get_file_size(const struct xblob_media_report_s* report, uint64_t* out_size);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_file_path(
    const struct xblob_media_report_s* report, char* buffer, size_t* inout_buffer_size);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_title(
    const struct xblob_media_report_s* report, char* buffer, size_t* inout_buffer_size);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_human_summary(
    const struct xblob_media_report_s* report, char* buffer, size_t* inout_buffer_size);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_title_id(
    const struct xblob_media_report_s* report, uint32_t* out_title_id, int* out_has_title_id);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_disk_number(
    const struct xblob_media_report_s* report, uint32_t* out_disk_number, int* out_has_disk_number);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_game_region(
    const struct xblob_media_report_s* report, uint32_t* out_game_region, int* out_has_game_region);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_entry_point(
    const struct xblob_media_report_s* report, uint32_t* out_entry_point, int* out_has_entry_point);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_section_count(
    const struct xblob_media_report_s* report, uint32_t* out_section_count);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_media_report_get_error_message(
    const struct xblob_media_report_s* report, char* buffer, size_t* inout_buffer_size);

/* --- Machine Session & Diagnostic API (Added in ABI 1.1) --- */

struct xblob_machine_s;
typedef struct xblob_machine_s* xblob_machine_t;

typedef enum xblob_machine_state_t {
    XBLOB_MACHINE_STATE_CREATED = 0,
    XBLOB_MACHINE_STATE_PREPARED = 1,
    XBLOB_MACHINE_STATE_PAUSED = 2,
    XBLOB_MACHINE_STATE_FAULTED = 3,
    XBLOB_MACHINE_STATE_STOPPED = 4,
    XBLOB_MACHINE_STATE_RUNNING = 5
} xblob_machine_state_t;

typedef struct xblob_prepare_diagnostic_t {
    uint32_t struct_size;
    xblob_machine_state_t state;
    uint32_t entry_point;
    uint32_t section_count;
    uint32_t headers_size;
    uint32_t image_size;
    char title_name[64];
    uint32_t title_id;
    uint64_t ram_size_bytes;
    int is_prepared;
    char error_message[256];
} xblob_prepare_diagnostic_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_create(xblob_machine_t* out_machine);

XBLOB_C_API_EXPORT void XBLOB_C_API_CALL xblob_machine_destroy(xblob_machine_t machine);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_prepare_xbe(xblob_machine_t machine, const char* path_utf8, size_t path_length,
                          xblob_prepare_diagnostic_t* out_diagnostic);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_get_state(const struct xblob_machine_s* machine, xblob_machine_state_t* out_state);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_diagnostic(
    const struct xblob_machine_s* machine, xblob_prepare_diagnostic_t* out_diagnostic);

/* --- Diagnostic Execution & Trace API (Added in ABI 1.2) --- */

typedef struct xblob_machine_execution_result_t {
    uint32_t struct_size;
    xblob_machine_state_t state;
    uint64_t instructions_executed;
    uint64_t cycles_consumed;
} xblob_machine_execution_result_t;

typedef struct xblob_trace_summary_t {
    uint32_t struct_size;
    uint64_t event_count;
    uint64_t dropped_count;
    uint64_t total_recorded;
} xblob_trace_summary_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_step(xblob_machine_t machine, uint64_t instruction_budget,
                   xblob_machine_execution_result_t* out_result);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_run_diagnostic(xblob_machine_t machine, uint64_t max_instructions,
                             uint64_t max_cycles, xblob_machine_execution_result_t* out_result);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_pause(xblob_machine_t machine);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_stop(xblob_machine_t machine);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_trace_summary(
    const struct xblob_machine_s* machine, xblob_trace_summary_t* out_summary);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_clear_trace(xblob_machine_t machine);

/* --- Framebuffer Presentation & GPU Diagnostics API (Added in ABI 1.3) --- */

typedef struct xblob_frame_metadata_t {
    uint32_t struct_size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t pixel_format;
    uint64_t sequence_number;
    uint64_t frame_cycle;
    uint32_t buffer_size;
    int is_valid;
} xblob_frame_metadata_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_frame_metadata(
    const struct xblob_machine_s* machine, xblob_frame_metadata_t* out_metadata);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_copy_frame_pixels(
    const struct xblob_machine_s* machine, uint8_t* buffer, size_t* inout_buffer_size);

/* --- Media Boot & VFS Browsing API (Added in ABI 1.4) --- */

typedef struct xblob_boot_report_t {
    uint32_t struct_size;
    xblob_media_type_t media_type;
    char default_xbe_path[256];
    char title_name[64];
    uint32_t title_id;
    uint32_t entry_point;
    uint32_t section_count;
    uint64_t media_size_bytes;
    int is_bootable;
    char error_message[256];
} xblob_boot_report_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_prepare_media(xblob_machine_t machine, const char* path_utf8, size_t path_length,
                            xblob_boot_report_t* out_report);

struct xblob_vfs_browser_s;
typedef struct xblob_vfs_browser_s* xblob_vfs_browser_t;

typedef struct xblob_dir_entry_t {
    char name[256];
    uint64_t size;
    int is_directory;
    uint32_t attributes;
} xblob_dir_entry_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_vfs_browser_create(
    const char* path_utf8, size_t path_length, xblob_vfs_browser_t* out_browser);

XBLOB_C_API_EXPORT void XBLOB_C_API_CALL xblob_vfs_browser_destroy(xblob_vfs_browser_t browser);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_vfs_browser_get_entry_count(xblob_vfs_browser_t browser, const char* dir_path_utf8,
                                  size_t dir_path_len, uint32_t* out_count);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_vfs_browser_list_entries(
    xblob_vfs_browser_t browser, const char* dir_path_utf8, size_t dir_path_len, uint32_t offset,
    uint32_t limit, xblob_dir_entry_t* out_entries, uint32_t* inout_count);

/* --- Experimental Title Execution & Compatibility Diagnostics API (Added in ABI 1.5) --- */

typedef enum xblob_stop_reason_code_t {
    XBLOB_STOP_REASON_NONE = 0,
    XBLOB_STOP_REASON_PAUSED = 1,
    XBLOB_STOP_REASON_STEP_COMPLETED = 2,
    XBLOB_STOP_REASON_HALTED = 3,
    XBLOB_STOP_REASON_BUDGET_INSTRUCTIONS = 4,
    XBLOB_STOP_REASON_BUDGET_CYCLES = 5,
    XBLOB_STOP_REASON_BUDGET_WALL_TIME = 6,
    XBLOB_STOP_REASON_BUDGET_EVENTS = 7,
    XBLOB_STOP_REASON_WATCHDOG_TIMEOUT = 8,
    XBLOB_STOP_REASON_UNSUPPORTED_OPCODE = 9,
    XBLOB_STOP_REASON_UNSUPPORTED_EXPORT = 10,
    XBLOB_STOP_REASON_UNSUPPORTED_GPU_METHOD = 11,
    XBLOB_STOP_REASON_UNSUPPORTED_FILE_SERVICE = 12,
    XBLOB_STOP_REASON_CPU_EXCEPTION = 13,
    XBLOB_STOP_REASON_MEMORY_FAULT = 14,
    XBLOB_STOP_REASON_INTERNAL_ERROR = 15
} xblob_stop_reason_code_t;

typedef struct xblob_execution_budgets_t {
    uint32_t struct_size;
    uint64_t max_instructions;
    uint64_t max_cycles;
    uint64_t max_wall_time_ms;
    uint64_t max_events;
    uint32_t chunk_instructions;
} xblob_execution_budgets_t;

typedef struct xblob_cpu_registers_snapshot_t {
    uint32_t struct_size;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;
    uint32_t eflags;
} xblob_cpu_registers_snapshot_t;

typedef struct xblob_machine_snapshot_t {
    uint32_t struct_size;
    xblob_machine_state_t state;
    xblob_stop_reason_code_t stop_reason_code;
    uint32_t fault_eip;
    uint32_t active_thread_id;
    uint32_t thread_count;
    uint64_t current_cycle;
    uint64_t instructions_executed;
    uint64_t events_fired;
    xblob_cpu_registers_snapshot_t registers;
    int stack_valid;
    uint32_t stack_words[8];
    char stop_reason_category[64];
    char stop_reason_symbol[64];
    char stop_reason_detail[256];
    char error_message[256];
} xblob_machine_snapshot_t;

typedef struct xblob_compatibility_diagnostic_t {
    uint32_t struct_size;
    xblob_stop_reason_code_t first_blocker_code;
    uint32_t blocker_ordinal_or_opcode;
    uint32_t blocker_thread_id;
    uint32_t blocker_eip;
    uint64_t blocker_count;
    char blocker_category[64];
    char blocker_symbol_or_mnemonic[64];
    char blocker_detail[256];
    uint64_t total_instructions;
    uint64_t total_cycles;
} xblob_compatibility_diagnostic_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_start_execution(xblob_machine_t machine, const xblob_execution_budgets_t* budgets);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_resume_execution(xblob_machine_t machine, const xblob_execution_budgets_t* budgets);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL
xblob_machine_wait_completion(xblob_machine_t machine, uint32_t timeout_ms, int* out_completed);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_snapshot(
    const struct xblob_machine_s* machine, xblob_machine_snapshot_t* out_snapshot);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_compatibility_diagnostic(
    const struct xblob_machine_s* machine, xblob_compatibility_diagnostic_t* out_diagnostic);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_trace_text(
    const struct xblob_machine_s* machine, char* buffer, size_t* inout_buffer_size);

/* --- Interactive Graphics & Input Session API (Added in ABI 1.6) --- */

typedef struct xblob_host_input_snapshot_t {
    uint32_t struct_size;
    uint64_t sequence;
    uint8_t connected;
    uint8_t digital_buttons;
    uint8_t button_a;
    uint8_t button_b;
    uint8_t button_x;
    uint8_t button_y;
    uint8_t button_black;
    uint8_t button_white;
    uint8_t trigger_left;
    uint8_t trigger_right;
    int16_t thumb_lx;
    int16_t thumb_ly;
    int16_t thumb_rx;
    int16_t thumb_ry;
    uint16_t padding;
} xblob_host_input_snapshot_t;

typedef struct xblob_interactive_metrics_t {
    uint32_t struct_size;
    uint64_t frame_sequence;
    uint64_t input_sequence;
    uint64_t instructions_executed;
    uint64_t cycles_consumed;
    uint32_t unsupported_gpu_count;
    uint32_t unsupported_usb_count;
    xblob_machine_state_t state;
    xblob_stop_reason_code_t stop_reason_code;
} xblob_interactive_metrics_t;

typedef struct xblob_unsupported_feature_entry_t {
    char subsystem[32];
    char capability[64];
    uint32_t identifier;
    uint64_t count;
    char first_context[128];
} xblob_unsupported_feature_entry_t;

typedef struct xblob_rumble_state_t {
    uint32_t struct_size;
    uint16_t left_motor;
    uint16_t right_motor;
} xblob_rumble_state_t;

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_submit_input(
    xblob_machine_t machine, const xblob_host_input_snapshot_t* snapshot, int* out_accepted);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_interactive_metrics(
    const struct xblob_machine_s* machine, xblob_interactive_metrics_t* out_metrics);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_rumble_state(
    const struct xblob_machine_s* machine, xblob_rumble_state_t* out_rumble);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_unsupported_features_count(
    const struct xblob_machine_s* machine, uint32_t* out_count);

XBLOB_C_API_EXPORT xblob_status_t XBLOB_C_API_CALL xblob_machine_get_unsupported_features(
    const struct xblob_machine_s* machine, uint32_t offset, uint32_t limit,
    xblob_unsupported_feature_entry_t* out_entries, uint32_t* inout_count);

#ifdef __cplusplus
}
#endif

#endif /* XBLOB_C_API_H */
