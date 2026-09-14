use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr;
use thiserror::Error;

use super::bindings::*;

#[derive(Error, Debug)]
pub enum FfiError {
    #[error("Argumento inválido passado para o núcleo C")]
    InvalidArgument,
    #[error("Arquivo não encontrado no caminho especificado")]
    NotFound,
    #[error("Formato de mídia não suportado")]
    UnsupportedFormat,
    #[error("Mídia ou cabeçalho corrompido")]
    CorruptMedia,
    #[error("Buffer fornecido é insuficiente")]
    BufferTooSmall,
    #[error("Versão de ABI C incompatível com o binário Rust")]
    IncompatibleVersion,
    #[error("Erro de entrada/saída ao acessar o arquivo")]
    IoError,
    #[error("Estado de máquina ou operação inválido")]
    InvalidState,
    #[error("Erro interno do núcleo C++ ({0})")]
    InternalError(String),
    #[error("Erro desconhecido na chamada FFI (código: {0})")]
    Unknown(XblobStatus),
}

impl FfiError {
    pub fn from_status(status: XblobStatus) -> Self {
        match status {
            XBLOB_STATUS_ERROR_INVALID_ARGUMENT | XBLOB_STATUS_ERROR_NULL_POINTER => {
                FfiError::InvalidArgument
            }
            XBLOB_STATUS_ERROR_NOT_FOUND => FfiError::NotFound,
            XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT => FfiError::UnsupportedFormat,
            XBLOB_STATUS_ERROR_CORRUPT_MEDIA => FfiError::CorruptMedia,
            XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL => FfiError::BufferTooSmall,
            XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION => FfiError::IncompatibleVersion,
            XBLOB_STATUS_ERROR_IO => FfiError::IoError,
            XBLOB_STATUS_ERROR_INVALID_STATE => FfiError::InvalidState,
            XBLOB_STATUS_ERROR_INTERNAL => {
                let msg = unsafe {
                    let ptr = xblob_status_to_string(status);
                    if ptr.is_null() {
                        "Falha interna".to_string()
                    } else {
                        CStr::from_ptr(ptr).to_string_lossy().into_owned()
                    }
                };
                FfiError::InternalError(msg)
            }
            other => FfiError::Unknown(other),
        }
    }

    pub fn code(&self) -> &'static str {
        match self {
            FfiError::InvalidArgument => "INVALID_ARGUMENT",
            FfiError::NotFound => "NOT_FOUND",
            FfiError::UnsupportedFormat => "UNSUPPORTED_FORMAT",
            FfiError::CorruptMedia => "CORRUPT_MEDIA",
            FfiError::BufferTooSmall => "BUFFER_TOO_SMALL",
            FfiError::IncompatibleVersion => "INCOMPATIBLE_VERSION",
            FfiError::IoError => "IO_ERROR",
            FfiError::InvalidState => "INVALID_STATE",
            FfiError::InternalError(_) => "INTERNAL_ERROR",
            FfiError::Unknown(_) => "UNKNOWN_ERROR",
        }
    }
}

pub struct SafeMediaReport {
    handle: XblobMediaReportHandle,
}

impl SafeMediaReport {
    pub fn inspect_file(path: &str) -> Result<Self, FfiError> {
        let path_bytes = path.as_bytes();
        let mut handle: XblobMediaReportHandle = ptr::null_mut();

        let status = unsafe {
            xblob_media_inspect(
                path_bytes.as_ptr() as *const c_char,
                path_bytes.len(),
                &mut handle,
            )
        };

        if status != XBLOB_STATUS_OK || handle.is_null() {
            return Err(FfiError::from_status(status));
        }

        Ok(SafeMediaReport { handle })
    }

    pub fn get_type(&self) -> Result<XblobMediaType, FfiError> {
        let mut media_type: XblobMediaType = XBLOB_MEDIA_TYPE_UNKNOWN;
        let status = unsafe { xblob_media_report_get_type(self.handle as _, &mut media_type) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(media_type)
    }

    pub fn get_file_size(&self) -> Result<u64, FfiError> {
        let mut size: u64 = 0;
        let status = unsafe { xblob_media_report_get_file_size(self.handle as _, &mut size) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(size)
    }

    fn read_two_call_string(
        &self,
        getter: unsafe extern "C" fn(
            *const XblobMediaReportOpaque,
            *mut c_char,
            usize,
            *mut usize,
        ) -> XblobStatus,
    ) -> Result<String, FfiError> {
        let mut required_size: usize = 0;
        let status = unsafe { getter(self.handle as _, ptr::null_mut(), 0, &mut required_size) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        if required_size <= 1 {
            return Ok(String::new());
        }

        let mut buf = vec![0u8; required_size];
        let status = unsafe {
            getter(
                self.handle as _,
                buf.as_mut_ptr() as *mut c_char,
                buf.len(),
                &mut required_size,
            )
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        let c_str = CStr::from_bytes_until_nul(&buf)
            .map_err(|_| FfiError::InternalError("String FFI não terminada em nulo".into()))?;
        Ok(c_str.to_string_lossy().into_owned())
    }

    pub fn get_file_path(&self) -> Result<String, FfiError> {
        self.read_two_call_string(xblob_media_report_get_file_path)
    }

    pub fn get_human_summary(&self) -> Result<String, FfiError> {
        self.read_two_call_string(xblob_media_report_get_human_summary)
    }

    pub fn get_title(&self) -> Result<String, FfiError> {
        self.read_two_call_string(xblob_media_report_get_title)
    }

    pub fn get_title_id(&self) -> Result<Option<u32>, FfiError> {
        let mut id = 0u32;
        let status = unsafe { xblob_media_report_get_title_id(self.handle as _, &mut id) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(Some(id))
    }

    pub fn get_disk_number(&self) -> Result<Option<u32>, FfiError> {
        let mut disk = 0u32;
        let status = unsafe { xblob_media_report_get_disk_number(self.handle as _, &mut disk) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(Some(disk))
    }

    pub fn get_game_region(&self) -> Result<Option<u32>, FfiError> {
        let mut region = 0u32;
        let status = unsafe { xblob_media_report_get_game_region(self.handle as _, &mut region) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(Some(region))
    }

    pub fn get_entry_point(&self) -> Result<Option<u32>, FfiError> {
        let mut ep = 0u32;
        let status = unsafe { xblob_media_report_get_entry_point(self.handle as _, &mut ep) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(Some(ep))
    }

    pub fn get_section_count(&self) -> Result<u32, FfiError> {
        let mut count = 0u32;
        let status = unsafe { xblob_media_report_get_section_count(self.handle as _, &mut count) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(count)
    }
}

impl Drop for SafeMediaReport {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                xblob_media_report_destroy(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}

pub fn get_core_info_safe() -> Result<(u32, u32, u32, u64, String, String), FfiError> {
    let mut raw_info = XblobCoreInfo {
        struct_size: std::mem::size_of::<XblobCoreInfo>() as u32,
        abi_version_major: 0,
        abi_version_minor: 0,
        abi_version_patch: 0,
        capabilities: 0,
        product_name: [0; 64],
        product_version: [0; 32],
    };

    let status = unsafe { xblob_get_core_info(&mut raw_info) };
    if status != XBLOB_STATUS_OK {
        return Err(FfiError::from_status(status));
    }

    let product_name = unsafe { CStr::from_ptr(raw_info.product_name.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    let product_version = unsafe { CStr::from_ptr(raw_info.product_version.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    Ok((
        raw_info.abi_version_major,
        raw_info.abi_version_minor,
        raw_info.abi_version_patch,
        raw_info.capabilities,
        product_name,
        product_version,
    ))
}

pub struct SafeMachineSession {
    handle: XblobMachineHandle,
}

impl SafeMachineSession {
    pub fn new() -> Result<Self, FfiError> {
        let mut handle: XblobMachineHandle = ptr::null_mut();
        let status = unsafe { xblob_machine_create(&mut handle) };
        if status != XBLOB_STATUS_OK || handle.is_null() {
            return Err(FfiError::from_status(status));
        }
        Ok(SafeMachineSession { handle })
    }

    pub fn prepare_xbe(&mut self, path: &str) -> Result<XblobPrepareDiagnostic, FfiError> {
        let path_bytes = path.as_bytes();
        let mut diag = XblobPrepareDiagnostic {
            struct_size: std::mem::size_of::<XblobPrepareDiagnostic>() as u32,
            state: XBLOB_MACHINE_STATE_CREATED,
            entry_point: 0,
            section_count: 0,
            headers_size: 0,
            image_size: 0,
            title_name: [0; 64],
            title_id: 0,
            ram_size_bytes: 0,
            is_prepared: 0,
            error_message: [0; 256],
        };

        let status = unsafe {
            xblob_machine_prepare_xbe(
                self.handle,
                path_bytes.as_ptr() as *const c_char,
                path_bytes.len(),
                &mut diag,
            )
        };

        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        Ok(diag)
    }

    pub fn get_state(&self) -> Result<XblobMachineState, FfiError> {
        let mut state: XblobMachineState = XBLOB_MACHINE_STATE_CREATED;
        let status = unsafe { xblob_machine_get_state(self.handle as _, &mut state) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(state)
    }

    pub fn get_diagnostic(&self) -> Result<XblobPrepareDiagnostic, FfiError> {
        let mut diag = XblobPrepareDiagnostic {
            struct_size: std::mem::size_of::<XblobPrepareDiagnostic>() as u32,
            state: XBLOB_MACHINE_STATE_CREATED,
            entry_point: 0,
            section_count: 0,
            headers_size: 0,
            image_size: 0,
            title_name: [0; 64],
            title_id: 0,
            ram_size_bytes: 0,
            is_prepared: 0,
            error_message: [0; 256],
        };

        let status = unsafe { xblob_machine_get_diagnostic(self.handle as _, &mut diag) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(diag)
    }

    pub fn get_frame_metadata(&self) -> Result<XblobFrameMetadata, FfiError> {
        let mut meta = XblobFrameMetadata {
            struct_size: std::mem::size_of::<XblobFrameMetadata>() as u32,
            width: 0,
            height: 0,
            pitch: 0,
            pixel_format: 0,
            sequence_number: 0,
            frame_cycle: 0,
            buffer_size: 0,
            is_valid: 0,
        };

        let status = unsafe { xblob_machine_get_frame_metadata(self.handle as _, &mut meta) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(meta)
    }

    pub fn copy_frame_pixels(&self, max_buffer_bytes: usize) -> Result<Vec<u8>, FfiError> {
        let mut required_size: usize = 0;
        let status = unsafe {
            xblob_machine_copy_frame_pixels(self.handle as _, ptr::null_mut(), &mut required_size)
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        if required_size == 0 {
            return Ok(Vec::new());
        }

        if required_size > max_buffer_bytes || required_size > 8_294_400 {
            return Err(FfiError::BufferTooSmall);
        }

        let mut buffer = vec![0u8; required_size];
        let mut inout_size = required_size;
        let status = unsafe {
            xblob_machine_copy_frame_pixels(self.handle as _, buffer.as_mut_ptr(), &mut inout_size)
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        buffer.truncate(inout_size);
        Ok(buffer)
    }

    pub fn prepare_media(&mut self, path: &str) -> Result<XblobBootReport, FfiError> {
        let path_bytes = path.as_bytes();
        let mut report = XblobBootReport {
            struct_size: std::mem::size_of::<XblobBootReport>() as u32,
            media_type: XBLOB_MEDIA_TYPE_UNKNOWN,
            default_xbe_path: [0; 256],
            title_name: [0; 64],
            title_id: 0,
            entry_point: 0,
            section_count: 0,
            media_size_bytes: 0,
            is_bootable: 0,
            error_message: [0; 256],
        };

        let status = unsafe {
            xblob_machine_prepare_media(
                self.handle,
                path_bytes.as_ptr() as *const c_char,
                path_bytes.len(),
                &mut report,
            )
        };

        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        Ok(report)
    }

    pub fn start_execution(
        &mut self,
        budgets: Option<XblobExecutionBudgets>,
    ) -> Result<(), FfiError> {
        let status = unsafe {
            match budgets {
                Some(b) => xblob_machine_start_execution(self.handle, &b),
                None => xblob_machine_start_execution(self.handle, ptr::null()),
            }
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(())
    }

    pub fn resume_execution(
        &mut self,
        budgets: Option<XblobExecutionBudgets>,
    ) -> Result<(), FfiError> {
        let status = unsafe {
            match budgets {
                Some(b) => xblob_machine_resume_execution(self.handle, &b),
                None => xblob_machine_resume_execution(self.handle, ptr::null()),
            }
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(())
    }

    pub fn pause(&mut self) -> Result<(), FfiError> {
        let status = unsafe { xblob_machine_pause(self.handle) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(())
    }

    pub fn stop(&mut self) -> Result<(), FfiError> {
        let status = unsafe { xblob_machine_stop(self.handle) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(())
    }

    pub fn wait_completion(&mut self, timeout_ms: u32) -> Result<bool, FfiError> {
        let mut completed: i32 = 0;
        let status =
            unsafe { xblob_machine_wait_completion(self.handle, timeout_ms, &mut completed) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(completed != 0)
    }

    pub fn get_snapshot(&self) -> Result<XblobMachineSnapshot, FfiError> {
        let mut snap = XblobMachineSnapshot {
            struct_size: std::mem::size_of::<XblobMachineSnapshot>() as u32,
            state: XBLOB_MACHINE_STATE_CREATED,
            stop_reason_code: XBLOB_STOP_REASON_NONE,
            fault_eip: 0,
            active_thread_id: 0,
            thread_count: 0,
            current_cycle: 0,
            instructions_executed: 0,
            events_fired: 0,
            registers: XblobCpuRegistersSnapshot {
                struct_size: std::mem::size_of::<XblobCpuRegistersSnapshot>() as u32,
                eax: 0,
                ecx: 0,
                edx: 0,
                ebx: 0,
                esp: 0,
                ebp: 0,
                esi: 0,
                edi: 0,
                eip: 0,
                eflags: 0,
            },
            stack_valid: 0,
            stack_words: [0; 8],
            stop_reason_category: [0; 64],
            stop_reason_symbol: [0; 64],
            stop_reason_detail: [0; 256],
            error_message: [0; 256],
        };

        let status = unsafe { xblob_machine_get_snapshot(self.handle as _, &mut snap) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(snap)
    }

    pub fn get_compatibility_diagnostic(&self) -> Result<XblobCompatibilityDiagnostic, FfiError> {
        let mut diag = XblobCompatibilityDiagnostic {
            struct_size: std::mem::size_of::<XblobCompatibilityDiagnostic>() as u32,
            first_blocker_code: XBLOB_STOP_REASON_NONE,
            blocker_ordinal_or_opcode: 0,
            blocker_thread_id: 0,
            blocker_eip: 0,
            blocker_count: 0,
            blocker_category: [0; 64],
            blocker_symbol_or_mnemonic: [0; 64],
            blocker_detail: [0; 256],
            total_instructions: 0,
            total_cycles: 0,
        };

        let status =
            unsafe { xblob_machine_get_compatibility_diagnostic(self.handle as _, &mut diag) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(diag)
    }

    pub fn get_trace_text(&self) -> Result<String, FfiError> {
        let mut required_size: usize = 0;
        let status = unsafe {
            xblob_machine_get_trace_text(self.handle as _, ptr::null_mut(), &mut required_size)
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        if required_size <= 1 {
            return Ok(String::new());
        }
        let mut buf = vec![0u8; required_size];
        let mut inout_size = required_size;
        let status = unsafe {
            xblob_machine_get_trace_text(
                self.handle as _,
                buf.as_mut_ptr() as *mut c_char,
                &mut inout_size,
            )
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        let c_str = CStr::from_bytes_until_nul(&buf).map_err(|_| {
            FfiError::InternalError("String trace FFI não terminada em nulo".into())
        })?;
        Ok(c_str.to_string_lossy().into_owned())
    }

    pub fn step(&self, budget: u64) -> Result<XblobMachineExecutionResult, FfiError> {
        let mut res = XblobMachineExecutionResult {
            struct_size: std::mem::size_of::<XblobMachineExecutionResult>() as u32,
            state: XBLOB_MACHINE_STATE_CREATED,
            instructions_executed: 0,
            cycles_consumed: 0,
        };
        let status = unsafe { xblob_machine_step(self.handle, budget, &mut res) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(res)
    }

    pub fn submit_input(&self, snapshot: &XblobHostInputSnapshot) -> Result<bool, FfiError> {
        let mut accepted: std::os::raw::c_int = 0;
        let status = unsafe { xblob_machine_submit_input(self.handle, snapshot, &mut accepted) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(accepted != 0)
    }

    pub fn get_interactive_metrics(&self) -> Result<XblobInteractiveMetrics, FfiError> {
        let mut metrics = XblobInteractiveMetrics {
            struct_size: std::mem::size_of::<XblobInteractiveMetrics>() as u32,
            frame_sequence: 0,
            input_sequence: 0,
            instructions_executed: 0,
            cycles_consumed: 0,
            unsupported_gpu_count: 0,
            unsupported_usb_count: 0,
            state: XBLOB_MACHINE_STATE_CREATED,
            stop_reason_code: XBLOB_STOP_REASON_NONE,
        };
        let status =
            unsafe { xblob_machine_get_interactive_metrics(self.handle as _, &mut metrics) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(metrics)
    }

    pub fn get_rumble_state(&self) -> Result<XblobRumbleState, FfiError> {
        let mut rumble = XblobRumbleState {
            struct_size: std::mem::size_of::<XblobRumbleState>() as u32,
            left_motor: 0,
            right_motor: 0,
        };
        let status = unsafe { xblob_machine_get_rumble_state(self.handle as _, &mut rumble) };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }
        Ok(rumble)
    }

    pub fn get_unsupported_features(
        &self,
        offset: u32,
        limit: u32,
    ) -> Result<Vec<XblobUnsupportedFeatureEntry>, FfiError> {
        let mut total_count: u32 = 0;
        let status = unsafe {
            xblob_machine_get_unsupported_features_count(self.handle as _, &mut total_count)
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        if total_count == 0 || offset >= total_count || limit == 0 {
            return Ok(Vec::new());
        }

        let fetch_count = (total_count - offset).min(limit);
        let dummy = XblobUnsupportedFeatureEntry {
            subsystem: [0; 32],
            capability: [0; 64],
            identifier: 0,
            count: 0,
            first_context: [0; 128],
        };
        let mut entries = vec![dummy; fetch_count as usize];
        let mut inout_count = fetch_count;

        let status = unsafe {
            xblob_machine_get_unsupported_features(
                self.handle as _,
                offset,
                limit,
                entries.as_mut_ptr(),
                &mut inout_count,
            )
        };
        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        entries.truncate(inout_count as usize);
        Ok(entries)
    }
}

impl Drop for SafeMachineSession {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                xblob_machine_stop(self.handle);
                xblob_machine_destroy(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}

unsafe impl Send for SafeMachineSession {}

pub struct SafeVfsBrowser {
    handle: XblobVfsBrowserHandle,
}

impl SafeVfsBrowser {
    pub fn open(path: &str) -> Result<Self, FfiError> {
        let path_bytes = path.as_bytes();
        let mut handle: XblobVfsBrowserHandle = ptr::null_mut();

        let status = unsafe {
            xblob_vfs_browser_create(
                path_bytes.as_ptr() as *const c_char,
                path_bytes.len(),
                &mut handle,
            )
        };

        if status != XBLOB_STATUS_OK || handle.is_null() {
            return Err(FfiError::from_status(status));
        }

        Ok(SafeVfsBrowser { handle })
    }

    pub fn get_entry_count(&self, dir_path: &str) -> Result<u32, FfiError> {
        let dir_bytes = dir_path.as_bytes();
        let mut count: u32 = 0;
        let status = unsafe {
            xblob_vfs_browser_get_entry_count(
                self.handle,
                dir_bytes.as_ptr() as *const c_char,
                dir_bytes.len(),
                &mut count,
            )
        };

        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        Ok(count)
    }

    pub fn list_entries(
        &self,
        dir_path: &str,
        offset: u32,
        limit: u32,
    ) -> Result<Vec<XblobDirEntry>, FfiError> {
        let dir_bytes = dir_path.as_bytes();
        let mut count: u32 = 0;
        let status = unsafe {
            xblob_vfs_browser_list_entries(
                self.handle,
                dir_bytes.as_ptr() as *const c_char,
                dir_bytes.len(),
                offset,
                limit,
                ptr::null_mut(),
                &mut count,
            )
        };

        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        if count == 0 {
            return Ok(Vec::new());
        }

        let mut entries = vec![
            XblobDirEntry {
                name: [0; 256],
                size: 0,
                is_directory: 0,
                attributes: 0,
            };
            count as usize
        ];

        let mut actual_count = count;
        let status = unsafe {
            xblob_vfs_browser_list_entries(
                self.handle,
                dir_bytes.as_ptr() as *const c_char,
                dir_bytes.len(),
                offset,
                limit,
                entries.as_mut_ptr(),
                &mut actual_count,
            )
        };

        if status != XBLOB_STATUS_OK {
            return Err(FfiError::from_status(status));
        }

        entries.truncate(actual_count as usize);
        Ok(entries)
    }
}

impl Drop for SafeVfsBrowser {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                xblob_vfs_browser_destroy(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}

const B64_CHARS: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

pub fn encode_base64(data: &[u8]) -> String {
    if data.is_empty() {
        return String::new();
    }
    let mut result = String::with_capacity(data.len().div_ceil(3) * 4);
    for chunk in data.chunks(3) {
        let b0 = chunk[0];
        let b1 = if chunk.len() > 1 { chunk[1] } else { 0 };
        let b2 = if chunk.len() > 2 { chunk[2] } else { 0 };

        result.push(B64_CHARS[(b0 >> 2) as usize] as char);
        result.push(B64_CHARS[(((b0 & 0x03) << 4) | (b1 >> 4)) as usize] as char);
        if chunk.len() > 1 {
            result.push(B64_CHARS[(((b1 & 0x0F) << 2) | (b2 >> 6)) as usize] as char);
        } else {
            result.push('=');
        }
        if chunk.len() > 2 {
            result.push(B64_CHARS[(b2 & 0x3F) as usize] as char);
        } else {
            result.push('=');
        }
    }
    result
}
