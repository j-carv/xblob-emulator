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
pub const XBLOB_C_API_VERSION_MINOR: u32 = 2;
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
