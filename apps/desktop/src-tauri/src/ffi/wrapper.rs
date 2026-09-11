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
}

impl Drop for SafeMachineSession {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                xblob_machine_destroy(self.handle);
            }
            self.handle = ptr::null_mut();
        }
    }
}
