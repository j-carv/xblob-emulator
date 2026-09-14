use std::os::raw::c_char;

pub type XblobStatus = i32;

pub const XBLOB_STATUS_OK: XblobStatus = 0;
pub const XBLOB_STATUS_ERROR_INVALID_ARGUMENT: XblobStatus = 1;
pub const XBLOB_STATUS_ERROR_NULL_POINTER: XblobStatus = 2;
pub const XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL: XblobStatus = 3;
pub const XBLOB_STATUS_ERROR_NOT_FOUND: XblobStatus = 4;
pub const XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT: XblobStatus = 5;
pub const XBLOB_STATUS_ERROR_CORRUPT_MEDIA: XblobStatus = 6;
pub const XBLOB_STATUS_ERROR_IO: XblobStatus = 7;
pub const XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION: XblobStatus = 8;
pub const XBLOB_STATUS_ERROR_INVALID_STATE: XblobStatus = 9;
pub const XBLOB_STATUS_ERROR_INTERNAL: XblobStatus = 99;

pub const XBLOB_C_API_VERSION_MAJOR: u32 = 1;
pub const XBLOB_C_API_VERSION_MINOR: u32 = 5;
pub const XBLOB_C_API_VERSION_PATCH: u32 = 0;

pub const XBLOB_CAPABILITY_NONE: u64 = 0;
pub const XBLOB_CAPABILITY_MEDIA_INSPECTION: u64 = 1 << 0;
pub const XBLOB_CAPABILITY_DETERMINISTIC_SCHEDULER: u64 = 1 << 1;
pub const XBLOB_CAPABILITY_GUEST_MEMORY: u64 = 1 << 2;
pub const XBLOB_CAPABILITY_SYNTHETIC_CPU: u64 = 1 << 3;
pub const XBLOB_CAPABILITY_GUEST_BUS: u64 = 1 << 4;
pub const XBLOB_CAPABILITY_VIRTUAL_MEMORY: u64 = 1 << 5;
pub const XBLOB_CAPABILITY_XBE_LOADER: u64 = 1 << 6;
pub const XBLOB_CAPABILITY_MACHINE_SESSION: u64 = 1 << 7;
pub const XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION: u64 = 1 << 8;
pub const XBLOB_CAPABILITY_FRAMEBUFFER_PRESENTATION: u64 = 1 << 9;
pub const XBLOB_CAPABILITY_NV2A_GPU: u64 = 1 << 10;
pub const XBLOB_CAPABILITY_XDVDFS_VFS: u64 = 1 << 11;
pub const XBLOB_CAPABILITY_MEDIA_BOOT: u64 = 1 << 12;
pub const XBLOB_CAPABILITY_EXPERIMENTAL_TITLE_EXECUTION: u64 = 1 << 13;
pub const XBLOB_CAPABILITY_COMPATIBILITY_DIAGNOSTICS: u64 = 1 << 14;

pub type XblobMediaType = i32;
pub const XBLOB_MEDIA_TYPE_UNKNOWN: XblobMediaType = 0;
pub const XBLOB_MEDIA_TYPE_XBE: XblobMediaType = 1;
pub const XBLOB_MEDIA_TYPE_XISO_TRIMMED: XblobMediaType = 2;
pub const XBLOB_MEDIA_TYPE_XISO_RAW: XblobMediaType = 3;
pub const XBLOB_MEDIA_TYPE_ISO9660_UNSUPPORTED: XblobMediaType = 4;

pub type XblobMachineState = i32;
pub const XBLOB_MACHINE_STATE_CREATED: XblobMachineState = 0;
pub const XBLOB_MACHINE_STATE_PREPARED: XblobMachineState = 1;
pub const XBLOB_MACHINE_STATE_PAUSED: XblobMachineState = 2;
pub const XBLOB_MACHINE_STATE_FAULTED: XblobMachineState = 3;
pub const XBLOB_MACHINE_STATE_STOPPED: XblobMachineState = 4;
pub const XBLOB_MACHINE_STATE_RUNNING: XblobMachineState = 5;

pub type XblobStopReasonCode = i32;
pub const XBLOB_STOP_REASON_NONE: XblobStopReasonCode = 0;
pub const XBLOB_STOP_REASON_PAUSED: XblobStopReasonCode = 1;
pub const XBLOB_STOP_REASON_STEP_COMPLETED: XblobStopReasonCode = 2;
pub const XBLOB_STOP_REASON_HALTED: XblobStopReasonCode = 3;
pub const XBLOB_STOP_REASON_BUDGET_INSTRUCTIONS: XblobStopReasonCode = 4;
pub const XBLOB_STOP_REASON_BUDGET_CYCLES: XblobStopReasonCode = 5;
pub const XBLOB_STOP_REASON_BUDGET_WALL_TIME: XblobStopReasonCode = 6;
pub const XBLOB_STOP_REASON_BUDGET_EVENTS: XblobStopReasonCode = 7;
pub const XBLOB_STOP_REASON_WATCHDOG_TIMEOUT: XblobStopReasonCode = 8;
pub const XBLOB_STOP_REASON_UNSUPPORTED_OPCODE: XblobStopReasonCode = 9;
pub const XBLOB_STOP_REASON_UNSUPPORTED_EXPORT: XblobStopReasonCode = 10;
pub const XBLOB_STOP_REASON_UNSUPPORTED_GPU_METHOD: XblobStopReasonCode = 11;
pub const XBLOB_STOP_REASON_UNSUPPORTED_FILE_SERVICE: XblobStopReasonCode = 12;
pub const XBLOB_STOP_REASON_CPU_EXCEPTION: XblobStopReasonCode = 13;
pub const XBLOB_STOP_REASON_MEMORY_FAULT: XblobStopReasonCode = 14;
pub const XBLOB_STOP_REASON_INTERNAL_ERROR: XblobStopReasonCode = 15;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct XblobPrepareDiagnostic {
    pub struct_size: u32,
    pub state: XblobMachineState,
    pub entry_point: u32,
    pub section_count: u32,
    pub headers_size: u32,
    pub image_size: u32,
    pub title_name: [c_char; 64],
    pub title_id: u32,
    pub ram_size_bytes: u64,
    pub is_prepared: i32,
    pub error_message: [c_char; 256],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct XblobBootReport {
    pub struct_size: u32,
    pub media_type: XblobMediaType,
    pub default_xbe_path: [c_char; 256],
    pub title_name: [c_char; 64],
    pub title_id: u32,
    pub entry_point: u32,
    pub section_count: u32,
    pub media_size_bytes: u64,
    pub is_bootable: i32,
    pub error_message: [c_char; 256],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct XblobDirEntry {
    pub name: [c_char; 256],
    pub size: u64,
    pub is_directory: i32,
    pub attributes: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct XblobCoreInfo {
    pub struct_size: u32,
    pub abi_version_major: u32,
    pub abi_version_minor: u32,
    pub abi_version_patch: u32,
    pub capabilities: u64,
    pub product_name: [c_char; 64],
    pub product_version: [c_char; 32],
}

pub enum XblobMediaReportOpaque {}
pub type XblobMediaReportHandle = *mut XblobMediaReportOpaque;

pub enum XblobMachineOpaque {}
pub type XblobMachineHandle = *mut XblobMachineOpaque;

pub enum XblobVfsBrowserOpaque {}
pub type XblobVfsBrowserHandle = *mut XblobVfsBrowserOpaque;

extern "C" {
    pub fn xblob_get_abi_version_major() -> u32;
    pub fn xblob_get_abi_version_minor() -> u32;
    pub fn xblob_get_abi_version_patch() -> u32;
    pub fn xblob_get_capabilities() -> u64;
    pub fn xblob_get_product_version() -> *const c_char;
    pub fn xblob_get_product_name() -> *const c_char;
    pub fn xblob_status_to_string(status: XblobStatus) -> *const c_char;
    pub fn xblob_get_core_info(out_info: *mut XblobCoreInfo) -> XblobStatus;

    pub fn xblob_media_inspect(
        path_utf8: *const c_char,
        path_length: usize,
        out_report: *mut XblobMediaReportHandle,
    ) -> XblobStatus;

    pub fn xblob_media_report_destroy(report: XblobMediaReportHandle);

    pub fn xblob_media_report_get_type(
        report: *const XblobMediaReportOpaque,
        out_type: *mut XblobMediaType,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_file_size(
        report: *const XblobMediaReportOpaque,
        out_size: *mut u64,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_file_path(
        report: *const XblobMediaReportOpaque,
        buffer: *mut c_char,
        buffer_size: usize,
        out_required_size: *mut usize,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_title(
        report: *const XblobMediaReportOpaque,
        buffer: *mut c_char,
        buffer_size: usize,
        out_required_size: *mut usize,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_human_summary(
        report: *const XblobMediaReportOpaque,
        buffer: *mut c_char,
        buffer_size: usize,
        out_required_size: *mut usize,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_title_id(
        report: *const XblobMediaReportOpaque,
        out_title_id: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_disk_number(
        report: *const XblobMediaReportOpaque,
        out_disk_number: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_game_region(
        report: *const XblobMediaReportOpaque,
        out_game_region: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_entry_point(
        report: *const XblobMediaReportOpaque,
        out_entry_point: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_media_report_get_section_count(
        report: *const XblobMediaReportOpaque,
        out_section_count: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_machine_create(out_machine: *mut XblobMachineHandle) -> XblobStatus;
    pub fn xblob_machine_destroy(machine: XblobMachineHandle);
    pub fn xblob_machine_prepare_xbe(
        machine: XblobMachineHandle,
        path_utf8: *const c_char,
        path_length: usize,
        out_diagnostic: *mut XblobPrepareDiagnostic,
    ) -> XblobStatus;
    pub fn xblob_machine_get_state(
        machine: *const XblobMachineOpaque,
        out_state: *mut XblobMachineState,
    ) -> XblobStatus;
    pub fn xblob_machine_get_diagnostic(
        machine: *const XblobMachineOpaque,
        out_diagnostic: *mut XblobPrepareDiagnostic,
    ) -> XblobStatus;

    pub fn xblob_machine_step(
        machine: XblobMachineHandle,
        instruction_budget: u64,
        out_result: *mut XblobMachineExecutionResult,
    ) -> XblobStatus;
    pub fn xblob_machine_run_diagnostic(
        machine: XblobMachineHandle,
        max_instructions: u64,
        max_cycles: u64,
        out_result: *mut XblobMachineExecutionResult,
    ) -> XblobStatus;
    pub fn xblob_machine_pause(machine: XblobMachineHandle) -> XblobStatus;
    pub fn xblob_machine_stop(machine: XblobMachineHandle) -> XblobStatus;
    pub fn xblob_machine_get_trace_summary(
        machine: *const XblobMachineOpaque,
        out_summary: *mut XblobTraceSummary,
    ) -> XblobStatus;
    pub fn xblob_machine_clear_trace(machine: XblobMachineHandle) -> XblobStatus;
    pub fn xblob_machine_get_frame_metadata(
        machine: *const XblobMachineOpaque,
        out_metadata: *mut XblobFrameMetadata,
    ) -> XblobStatus;
    pub fn xblob_machine_copy_frame_pixels(
        machine: *const XblobMachineOpaque,
        buffer: *mut u8,
        inout_buffer_size: *mut usize,
    ) -> XblobStatus;

    pub fn xblob_machine_prepare_media(
        machine: XblobMachineHandle,
        path_utf8: *const c_char,
        path_length: usize,
        out_report: *mut XblobBootReport,
    ) -> XblobStatus;

    pub fn xblob_vfs_browser_create(
        path_utf8: *const c_char,
        path_length: usize,
        out_browser: *mut XblobVfsBrowserHandle,
    ) -> XblobStatus;

    pub fn xblob_vfs_browser_destroy(browser: XblobVfsBrowserHandle);

    pub fn xblob_vfs_browser_get_entry_count(
        browser: XblobVfsBrowserHandle,
        dir_path_utf8: *const c_char,
        dir_path_len: usize,
        out_count: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_vfs_browser_list_entries(
        browser: XblobVfsBrowserHandle,
        dir_path_utf8: *const c_char,
        dir_path_len: usize,
        offset: u32,
        limit: u32,
        out_entries: *mut XblobDirEntry,
        inout_count: *mut u32,
    ) -> XblobStatus;

    pub fn xblob_machine_start_execution(
        machine: XblobMachineHandle,
        budgets: *const XblobExecutionBudgets,
    ) -> XblobStatus;

    pub fn xblob_machine_resume_execution(
        machine: XblobMachineHandle,
        budgets: *const XblobExecutionBudgets,
    ) -> XblobStatus;

    pub fn xblob_machine_wait_completion(
        machine: XblobMachineHandle,
        timeout_ms: u32,
        out_completed: *mut i32,
    ) -> XblobStatus;

    pub fn xblob_machine_get_snapshot(
        machine: *const XblobMachineOpaque,
        out_snapshot: *mut XblobMachineSnapshot,
    ) -> XblobStatus;

    pub fn xblob_machine_get_compatibility_diagnostic(
        machine: *const XblobMachineOpaque,
        out_diagnostic: *mut XblobCompatibilityDiagnostic,
    ) -> XblobStatus;

    pub fn xblob_machine_get_trace_text(
        machine: *const XblobMachineOpaque,
        buffer: *mut c_char,
        inout_buffer_size: *mut usize,
    ) -> XblobStatus;
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobExecutionBudgets {
    pub struct_size: u32,
    pub max_instructions: u64,
    pub max_cycles: u64,
    pub max_wall_time_ms: u64,
    pub max_events: u64,
    pub chunk_instructions: u32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobCpuRegistersSnapshot {
    pub struct_size: u32,
    pub eax: u32,
    pub ecx: u32,
    pub edx: u32,
    pub ebx: u32,
    pub esp: u32,
    pub ebp: u32,
    pub esi: u32,
    pub edi: u32,
    pub eip: u32,
    pub eflags: u32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobMachineSnapshot {
    pub struct_size: u32,
    pub state: XblobMachineState,
    pub stop_reason_code: XblobStopReasonCode,
    pub fault_eip: u32,
    pub active_thread_id: u32,
    pub thread_count: u32,
    pub current_cycle: u64,
    pub instructions_executed: u64,
    pub events_fired: u64,
    pub registers: XblobCpuRegistersSnapshot,
    pub stack_valid: i32,
    pub stack_words: [u32; 8],
    pub stop_reason_category: [c_char; 64],
    pub stop_reason_symbol: [c_char; 64],
    pub stop_reason_detail: [c_char; 256],
    pub error_message: [c_char; 256],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobCompatibilityDiagnostic {
    pub struct_size: u32,
    pub first_blocker_code: XblobStopReasonCode,
    pub blocker_ordinal_or_opcode: u32,
    pub blocker_thread_id: u32,
    pub blocker_eip: u32,
    pub blocker_count: u64,
    pub blocker_category: [c_char; 64],
    pub blocker_symbol_or_mnemonic: [c_char; 64],
    pub blocker_detail: [c_char; 256],
    pub total_instructions: u64,
    pub total_cycles: u64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobFrameMetadata {
    pub struct_size: u32,
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub pixel_format: u32,
    pub sequence_number: u64,
    pub frame_cycle: u64,
    pub buffer_size: u32,
    pub is_valid: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobMachineExecutionResult {
    pub struct_size: u32,
    pub state: XblobMachineState,
    pub instructions_executed: u64,
    pub cycles_consumed: u64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct XblobTraceSummary {
    pub struct_size: u32,
    pub event_count: u64,
    pub dropped_count: u64,
    pub total_recorded: u64,
}
