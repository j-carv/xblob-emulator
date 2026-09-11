use crate::dto::*;
use crate::ffi::*;

#[tauri::command]
pub fn get_core_info() -> Result<CoreInfoDto, AppErrorDto> {
    let (major, minor, patch, caps, name, ver) = get_core_info_safe().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    Ok(CoreInfoDto {
        abi_version_major: major,
        abi_version_minor: minor,
        abi_version_patch: patch,
        capabilities: caps,
        product_name: name,
        product_version: ver,
    })
}

#[tauri::command]
pub fn inspect_media(path: String) -> Result<MediaReportDto, AppErrorDto> {
    let report = SafeMediaReport::inspect_file(&path).map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let raw_type = report.get_type().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let media_type = match raw_type {
        XBLOB_MEDIA_TYPE_XBE => MediaTypeDto::Xbe,
        XBLOB_MEDIA_TYPE_XISO_TRIMMED => MediaTypeDto::XisoTrimmed,
        XBLOB_MEDIA_TYPE_XISO_RAW => MediaTypeDto::XisoRaw,
        XBLOB_MEDIA_TYPE_ISO9660_UNSUPPORTED => MediaTypeDto::Iso9660Unsupported,
        _ => MediaTypeDto::Unknown,
    };

    let file_size = report.get_file_size().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let file_path = report.get_file_path().unwrap_or_else(|_| path.clone());
    let human_summary = report.get_human_summary().unwrap_or_default();

    let xbe = if raw_type == XBLOB_MEDIA_TYPE_XBE {
        let title_name = report.get_title().unwrap_or_default();
        let title_id = report.get_title_id().unwrap_or(None);
        let title_id_hex = title_id.map(|id| format!("0x{:08X}", id));
        let disk_number = report.get_disk_number().unwrap_or(None);
        let game_region = report.get_game_region().unwrap_or(None);
        let entry_point = report.get_entry_point().unwrap_or(None);
        let entry_point_hex = entry_point.map(|ep| format!("0x{:08X}", ep));
        let section_count = report.get_section_count().unwrap_or(0);

        Some(XbeMetadataDto {
            title_name,
            title_id,
            title_id_hex,
            disk_number,
            game_region,
            entry_point,
            entry_point_hex,
            section_count,
        })
    } else {
        None
    };

    let xiso = if raw_type == XBLOB_MEDIA_TYPE_XISO_TRIMMED || raw_type == XBLOB_MEDIA_TYPE_XISO_RAW
    {
        let variant = match raw_type {
            XBLOB_MEDIA_TYPE_XISO_TRIMMED => "XisoTrimmed".to_string(),
            XBLOB_MEDIA_TYPE_XISO_RAW => "XisoRaw".to_string(),
            _ => "Unknown".to_string(),
        };

        Some(XisoMetadataDto {
            variant,
            volume_descriptor_offset: None,
            root_dir_sector: None,
            root_dir_size: None,
            valid_footer_magic: Some(true),
        })
    } else {
        None
    };

    Ok(MediaReportDto {
        media_type,
        file_path,
        file_size,
        human_summary,
        xbe,
        xiso,
    })
}

fn machine_state_to_string(state: XblobMachineState) -> &'static str {
    match state {
        XBLOB_MACHINE_STATE_CREATED => "Created",
        XBLOB_MACHINE_STATE_PREPARED => "Prepared",
        XBLOB_MACHINE_STATE_PAUSED => "Paused",
        XBLOB_MACHINE_STATE_FAULTED => "Faulted",
        XBLOB_MACHINE_STATE_STOPPED => "Stopped",
        _ => "Unknown",
    }
}

#[tauri::command]
pub fn prepare_machine_diagnostic(
    path: String,
) -> Result<MachinePrepareDiagnosticDto, AppErrorDto> {
    let mut session = SafeMachineSession::new().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let diag = session.prepare_xbe(&path).map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let title_name = unsafe { std::ffi::CStr::from_ptr(diag.title_name.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let error_msg = unsafe { std::ffi::CStr::from_ptr(diag.error_message.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    Ok(MachinePrepareDiagnosticDto {
        state: machine_state_to_string(diag.state).to_string(),
        entry_point: diag.entry_point,
        entry_point_hex: format!("0x{:08X}", diag.entry_point),
        section_count: diag.section_count,
        headers_size: diag.headers_size,
        image_size: diag.image_size,
        title_name,
        title_id: diag.title_id,
        title_id_hex: format!("0x{:08X}", diag.title_id),
        ram_size_bytes: diag.ram_size_bytes,
        is_prepared: diag.is_prepared != 0,
        error_message: if error_msg.is_empty() {
            None
        } else {
            Some(error_msg)
        },
    })
}
