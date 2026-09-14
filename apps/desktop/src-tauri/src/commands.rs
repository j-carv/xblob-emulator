use std::ffi::CStr;

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
        XBLOB_MACHINE_STATE_RUNNING => "Running",
        _ => "Unknown",
    }
}

fn stop_reason_code_to_string(code: XblobStopReasonCode) -> &'static str {
    match code {
        XBLOB_STOP_REASON_NONE => "None",
        XBLOB_STOP_REASON_PAUSED => "Paused",
        XBLOB_STOP_REASON_STEP_COMPLETED => "StepCompleted",
        XBLOB_STOP_REASON_HALTED => "Halted",
        XBLOB_STOP_REASON_BUDGET_INSTRUCTIONS => "BudgetInstructions",
        XBLOB_STOP_REASON_BUDGET_CYCLES => "BudgetCycles",
        XBLOB_STOP_REASON_BUDGET_WALL_TIME => "BudgetWallTime",
        XBLOB_STOP_REASON_BUDGET_EVENTS => "BudgetEvents",
        XBLOB_STOP_REASON_WATCHDOG_TIMEOUT => "WatchdogTimeout",
        XBLOB_STOP_REASON_UNSUPPORTED_OPCODE => "UnsupportedOpcode",
        XBLOB_STOP_REASON_UNSUPPORTED_EXPORT => "UnsupportedExport",
        XBLOB_STOP_REASON_UNSUPPORTED_GPU_METHOD => "UnsupportedGpuMethod",
        XBLOB_STOP_REASON_UNSUPPORTED_FILE_SERVICE => "UnsupportedFileService",
        XBLOB_STOP_REASON_CPU_EXCEPTION => "CpuException",
        XBLOB_STOP_REASON_MEMORY_FAULT => "MemoryFault",
        XBLOB_STOP_REASON_INTERNAL_ERROR => "InternalError",
        _ => "Unknown",
    }
}

fn raw_snapshot_to_dto(snap: &XblobMachineSnapshot) -> MachineSnapshotDto {
    let err_msg = unsafe { std::ffi::CStr::from_ptr(snap.error_message.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let cat = unsafe { std::ffi::CStr::from_ptr(snap.stop_reason_category.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let sym = unsafe { std::ffi::CStr::from_ptr(snap.stop_reason_symbol.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let det = unsafe { std::ffi::CStr::from_ptr(snap.stop_reason_detail.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    let mut stack_words = Vec::new();
    let mut stack_words_hex = Vec::new();
    if snap.stack_valid != 0 {
        for &w in &snap.stack_words {
            stack_words.push(w);
            stack_words_hex.push(format!("0x{:08X}", w));
        }
    }

    MachineSnapshotDto {
        state: machine_state_to_string(snap.state).to_string(),
        stop_reason_code: stop_reason_code_to_string(snap.stop_reason_code).to_string(),
        fault_eip: snap.fault_eip,
        fault_eip_hex: format!("0x{:08X}", snap.fault_eip),
        active_thread_id: snap.active_thread_id,
        thread_count: snap.thread_count,
        current_cycle: snap.current_cycle,
        instructions_executed: snap.instructions_executed,
        events_fired: snap.events_fired,
        registers: CpuRegistersDto {
            eax: snap.registers.eax,
            eax_hex: format!("0x{:08X}", snap.registers.eax),
            ecx: snap.registers.ecx,
            ecx_hex: format!("0x{:08X}", snap.registers.ecx),
            edx: snap.registers.edx,
            edx_hex: format!("0x{:08X}", snap.registers.edx),
            ebx: snap.registers.ebx,
            ebx_hex: format!("0x{:08X}", snap.registers.ebx),
            esp: snap.registers.esp,
            esp_hex: format!("0x{:08X}", snap.registers.esp),
            ebp: snap.registers.ebp,
            ebp_hex: format!("0x{:08X}", snap.registers.ebp),
            esi: snap.registers.esi,
            esi_hex: format!("0x{:08X}", snap.registers.esi),
            edi: snap.registers.edi,
            edi_hex: format!("0x{:08X}", snap.registers.edi),
            eip: snap.registers.eip,
            eip_hex: format!("0x{:08X}", snap.registers.eip),
            eflags: snap.registers.eflags,
            eflags_hex: format!("0x{:08X}", snap.registers.eflags),
        },
        stack_valid: snap.stack_valid != 0,
        stack_words,
        stack_words_hex,
        stop_reason_category: cat,
        stop_reason_symbol: sym,
        stop_reason_detail: det,
        error_message: if err_msg.is_empty() {
            None
        } else {
            Some(err_msg)
        },
    }
}

fn raw_diagnostic_to_dto(diag: &XblobCompatibilityDiagnostic) -> CompatibilityDiagnosticDto {
    let cat = unsafe { std::ffi::CStr::from_ptr(diag.blocker_category.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let sym = unsafe { std::ffi::CStr::from_ptr(diag.blocker_symbol_or_mnemonic.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let det = unsafe { std::ffi::CStr::from_ptr(diag.blocker_detail.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    CompatibilityDiagnosticDto {
        first_blocker_code: stop_reason_code_to_string(diag.first_blocker_code).to_string(),
        blocker_ordinal_or_opcode: diag.blocker_ordinal_or_opcode,
        blocker_ordinal_or_opcode_hex: format!("0x{:08X}", diag.blocker_ordinal_or_opcode),
        blocker_thread_id: diag.blocker_thread_id,
        blocker_eip: diag.blocker_eip,
        blocker_eip_hex: format!("0x{:08X}", diag.blocker_eip),
        blocker_count: diag.blocker_count,
        blocker_category: cat,
        blocker_symbol_or_mnemonic: sym,
        blocker_detail: det,
        total_instructions: diag.total_instructions,
        total_cycles: diag.total_cycles,
    }
}

fn dto_to_budgets(dto: Option<ExecutionBudgetsDto>) -> XblobExecutionBudgets {
    let d = dto.unwrap_or(ExecutionBudgetsDto {
        max_instructions: Some(10_000_000),
        max_cycles: Some(100_000_000),
        max_wall_time_ms: Some(5_000),
        max_events: Some(1_000_000),
        chunk_instructions: Some(1_000),
    });
    XblobExecutionBudgets {
        struct_size: std::mem::size_of::<XblobExecutionBudgets>() as u32,
        max_instructions: d.max_instructions.unwrap_or(10_000_000),
        max_cycles: d.max_cycles.unwrap_or(100_000_000),
        max_wall_time_ms: d.max_wall_time_ms.unwrap_or(5_000),
        max_events: d.max_events.unwrap_or(1_000_000),
        chunk_instructions: d.chunk_instructions.unwrap_or(1_000),
    }
}

static ACTIVE_SESSION: std::sync::Mutex<Option<SafeMachineSession>> = std::sync::Mutex::new(None);

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

#[tauri::command]
pub fn get_diagnostic_frame_snapshot(path: String) -> Result<GpuFrameSnapshotDto, AppErrorDto> {
    let mut session = SafeMachineSession::new().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    session.prepare_xbe(&path).map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let meta = session.get_frame_metadata().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let raw_pixels = session
        .copy_frame_pixels(8_294_400)
        .map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

    let pixels_base64 = encode_base64(&raw_pixels);

    Ok(GpuFrameSnapshotDto {
        metadata: GpuFrameMetadataDto {
            width: meta.width,
            height: meta.height,
            pitch: meta.pitch,
            pixel_format: meta.pixel_format,
            sequence_number: meta.sequence_number,
            frame_cycle: meta.frame_cycle,
            buffer_size: meta.buffer_size,
            is_valid: meta.is_valid != 0,
        },
        pixels_base64,
    })
}

#[tauri::command]
pub async fn prepare_media(path: String) -> Result<BootReportDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let mut session = SafeMachineSession::new().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let report = session.prepare_media(&path).map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let default_xbe_path = unsafe {
            std::ffi::CStr::from_ptr(report.default_xbe_path.as_ptr())
                .to_string_lossy()
                .into_owned()
        };

        let title_name = unsafe {
            std::ffi::CStr::from_ptr(report.title_name.as_ptr())
                .to_string_lossy()
                .into_owned()
        };

        let error_message = if report.error_message[0] != 0 {
            Some(unsafe {
                std::ffi::CStr::from_ptr(report.error_message.as_ptr())
                    .to_string_lossy()
                    .into_owned()
            })
        } else {
            None
        };

        let media_type_str = match report.media_type {
            XBLOB_MEDIA_TYPE_XBE => "xbe",
            XBLOB_MEDIA_TYPE_XISO_TRIMMED => "xiso_trimmed",
            XBLOB_MEDIA_TYPE_XISO_RAW => "xiso_raw",
            _ => "unknown",
        }
        .to_string();

        Ok(BootReportDto {
            media_type: media_type_str,
            default_xbe_path,
            title_name,
            title_id: report.title_id,
            title_id_hex: format!("0x{:08X}", report.title_id),
            entry_point: report.entry_point,
            entry_point_hex: format!("0x{:08X}", report.entry_point),
            section_count: report.section_count,
            media_size_bytes: report.media_size_bytes,
            is_bootable: report.is_bootable != 0,
            error_message,
        })
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub async fn browse_media_vfs(
    path: String,
    directory: String,
    offset: u32,
    limit: u32,
) -> Result<VfsDirectoryPageDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let browser = SafeVfsBrowser::open(&path).map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let total_count = browser
            .get_entry_count(&directory)
            .map_err(|e| AppErrorDto {
                code: e.code().to_string(),
                message: e.to_string(),
                details: None,
            })?;

        let raw_entries = browser
            .list_entries(&directory, offset, limit)
            .map_err(|e| AppErrorDto {
                code: e.code().to_string(),
                message: e.to_string(),
                details: None,
            })?;

        let entries = raw_entries
            .into_iter()
            .map(|e| {
                let name = unsafe {
                    std::ffi::CStr::from_ptr(e.name.as_ptr())
                        .to_string_lossy()
                        .into_owned()
                };
                VfsEntryDto {
                    name,
                    size: e.size,
                    is_directory: e.is_directory != 0,
                    attributes: e.attributes,
                }
            })
            .collect();

        Ok(VfsDirectoryPageDto {
            total_count,
            offset,
            limit,
            entries,
        })
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub async fn start_title_execution(
    path: String,
    budgets: Option<ExecutionBudgetsDto>,
) -> Result<MachineSnapshotDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let mut session = SafeMachineSession::new().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        // Try prepare_media first (for ISO/XISO/XBE), fallback to prepare_xbe
        if session.prepare_media(&path).is_err() {
            session.prepare_xbe(&path).map_err(|e| AppErrorDto {
                code: e.code().to_string(),
                message: e.to_string(),
                details: None,
            })?;
        }

        let b = dto_to_budgets(budgets);
        session.start_execution(Some(b)).map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let timeout = (b.max_wall_time_ms as u32).saturating_add(500);
        let _ = session.wait_completion(timeout);

        let snap = session.get_snapshot().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let mut lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
            code: "LOCK_ERROR".to_string(),
            message: "Falha ao adquirir lock de sessão".to_string(),
            details: None,
        })?;
        *lock = Some(session);

        Ok(raw_snapshot_to_dto(&snap))
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub async fn resume_title_execution(
    budgets: Option<ExecutionBudgetsDto>,
) -> Result<MachineSnapshotDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let mut lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
            code: "LOCK_ERROR".to_string(),
            message: "Falha ao adquirir lock de sessão".to_string(),
            details: None,
        })?;
        let session = lock.as_mut().ok_or_else(|| AppErrorDto {
            code: "NO_ACTIVE_SESSION".to_string(),
            message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
            details: None,
        })?;

        let b = dto_to_budgets(budgets);
        session.resume_execution(Some(b)).map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let timeout = (b.max_wall_time_ms as u32).saturating_add(500);
        let _ = session.wait_completion(timeout);

        let snap = session.get_snapshot().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        Ok(raw_snapshot_to_dto(&snap))
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub async fn pause_title_execution() -> Result<MachineSnapshotDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let mut lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
            code: "LOCK_ERROR".to_string(),
            message: "Falha ao adquirir lock de sessão".to_string(),
            details: None,
        })?;
        let session = lock.as_mut().ok_or_else(|| AppErrorDto {
            code: "NO_ACTIVE_SESSION".to_string(),
            message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
            details: None,
        })?;

        session.pause().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let snap = session.get_snapshot().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        Ok(raw_snapshot_to_dto(&snap))
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub async fn stop_title_execution() -> Result<MachineSnapshotDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let mut lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
            code: "LOCK_ERROR".to_string(),
            message: "Falha ao adquirir lock de sessão".to_string(),
            details: None,
        })?;
        let session = lock.as_mut().ok_or_else(|| AppErrorDto {
            code: "NO_ACTIVE_SESSION".to_string(),
            message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
            details: None,
        })?;

        session.stop().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let snap = session.get_snapshot().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        Ok(raw_snapshot_to_dto(&snap))
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}

#[tauri::command]
pub fn get_execution_snapshot() -> Result<MachineSnapshotDto, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    let snap = session.get_snapshot().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    Ok(raw_snapshot_to_dto(&snap))
}

#[tauri::command]
pub fn get_compatibility_diagnostic() -> Result<CompatibilityDiagnosticDto, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    let diag = session
        .get_compatibility_diagnostic()
        .map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

    Ok(raw_diagnostic_to_dto(&diag))
}

#[tauri::command]
pub fn get_execution_trace() -> Result<String, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    session.get_trace_text().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })
}

#[tauri::command]
pub fn submit_host_input(snapshot: HostInputSnapshotDto) -> Result<bool, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    let raw_snapshot = XblobHostInputSnapshot {
        struct_size: std::mem::size_of::<XblobHostInputSnapshot>() as u32,
        sequence: snapshot.sequence,
        connected: if snapshot.connected { 1 } else { 0 },
        digital_buttons: snapshot.digital_buttons,
        button_a: snapshot.button_a,
        button_b: snapshot.button_b,
        button_x: snapshot.button_x,
        button_y: snapshot.button_y,
        button_black: snapshot.button_black,
        button_white: snapshot.button_white,
        trigger_left: snapshot.trigger_left,
        trigger_right: snapshot.trigger_right,
        thumb_lx: snapshot.thumb_lx,
        thumb_ly: snapshot.thumb_ly,
        thumb_rx: snapshot.thumb_rx,
        thumb_ry: snapshot.thumb_ry,
        padding: 0,
    };

    session
        .submit_input(&raw_snapshot)
        .map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })
}

#[tauri::command]
pub fn get_interactive_frame(
    last_frame_sequence: u64,
    fetch_pixels: bool,
) -> Result<InteractiveFrameDto, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    let meta = session.get_frame_metadata().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let metrics_raw = session.get_interactive_metrics().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let rumble_raw = session.get_rumble_state().map_err(|e| AppErrorDto {
        code: e.code().to_string(),
        message: e.to_string(),
        details: None,
    })?;

    let has_new_frame = meta.is_valid != 0 && meta.sequence_number > last_frame_sequence;
    let pixels_base64 = if has_new_frame && fetch_pixels {
        let raw_pixels = session
            .copy_frame_pixels(8_294_400)
            .map_err(|e| AppErrorDto {
                code: e.code().to_string(),
                message: e.to_string(),
                details: None,
            })?;
        if raw_pixels.is_empty() {
            None
        } else {
            Some(encode_base64(&raw_pixels))
        }
    } else {
        None
    };

    Ok(InteractiveFrameDto {
        frame_sequence: meta.sequence_number,
        input_sequence: metrics_raw.input_sequence,
        width: meta.width,
        height: meta.height,
        pitch: meta.pitch,
        pixel_format: meta.pixel_format,
        has_new_frame,
        pixels_base64,
        rumble: RumbleStateDto {
            left_motor: rumble_raw.left_motor,
            right_motor: rumble_raw.right_motor,
        },
        metrics: InteractiveMetricsDto {
            frame_sequence: metrics_raw.frame_sequence,
            input_sequence: metrics_raw.input_sequence,
            instructions_executed: metrics_raw.instructions_executed,
            cycles_consumed: metrics_raw.cycles_consumed,
            unsupported_gpu_count: metrics_raw.unsupported_gpu_count,
            unsupported_usb_count: metrics_raw.unsupported_usb_count,
            state: machine_state_to_string(metrics_raw.state).to_string(),
            stop_reason: stop_reason_code_to_string(metrics_raw.stop_reason_code).to_string(),
        },
    })
}

#[tauri::command]
pub fn get_unsupported_features(
    offset: u32,
    limit: u32,
) -> Result<Vec<UnsupportedFeatureEntryDto>, AppErrorDto> {
    let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
        code: "LOCK_ERROR".to_string(),
        message: "Falha ao adquirir lock de sessão".to_string(),
        details: None,
    })?;
    let session = lock.as_ref().ok_or_else(|| AppErrorDto {
        code: "NO_ACTIVE_SESSION".to_string(),
        message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
        details: None,
    })?;

    let raw_entries = session
        .get_unsupported_features(offset, limit)
        .map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

    let dtos = raw_entries
        .into_iter()
        .map(|e| {
            let subsystem = unsafe {
                CStr::from_ptr(e.subsystem.as_ptr())
                    .to_string_lossy()
                    .into_owned()
            };
            let capability = unsafe {
                CStr::from_ptr(e.capability.as_ptr())
                    .to_string_lossy()
                    .into_owned()
            };
            let first_context = unsafe {
                CStr::from_ptr(e.first_context.as_ptr())
                    .to_string_lossy()
                    .into_owned()
            };
            UnsupportedFeatureEntryDto {
                subsystem,
                capability,
                identifier: e.identifier,
                identifier_hex: format!("0x{:04X}", e.identifier),
                count: e.count,
                first_context,
            }
        })
        .collect();

    Ok(dtos)
}

#[tauri::command]
pub async fn step_title_execution(
    instruction_budget: Option<u64>,
) -> Result<MachineSnapshotDto, AppErrorDto> {
    tauri::async_runtime::spawn_blocking(move || {
        let lock = ACTIVE_SESSION.lock().map_err(|_| AppErrorDto {
            code: "LOCK_ERROR".to_string(),
            message: "Falha ao adquirir lock de sessão".to_string(),
            details: None,
        })?;
        let session = lock.as_ref().ok_or_else(|| AppErrorDto {
            code: "NO_ACTIVE_SESSION".to_string(),
            message: "Nenhuma sessão de máquina ativa encontrada".to_string(),
            details: None,
        })?;

        let budget = instruction_budget.unwrap_or(1);
        session.step(budget).map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        let snap = session.get_snapshot().map_err(|e| AppErrorDto {
            code: e.code().to_string(),
            message: e.to_string(),
            details: None,
        })?;

        Ok(raw_snapshot_to_dto(&snap))
    })
    .await
    .map_err(|e| AppErrorDto {
        code: "INTERNAL_ERROR".to_string(),
        message: e.to_string(),
        details: None,
    })?
}
