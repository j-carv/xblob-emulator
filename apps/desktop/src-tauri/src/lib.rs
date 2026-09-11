pub mod commands;
pub mod dto;
pub mod ffi;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![
            commands::get_core_info,
            commands::inspect_media,
            commands::prepare_machine_diagnostic,
            commands::get_diagnostic_frame_snapshot,
            commands::prepare_media,
            commands::browse_media_vfs
        ])
        .run(tauri::generate_context!())
        .expect("error while running xblob desktop application");
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::NamedTempFile;

    #[test]
    fn test_core_info_ffi_and_dto() {
        let result = commands::get_core_info();
        assert!(result.is_ok(), "get_core_info should succeed");

        let info = result.unwrap();
        assert_eq!(info.abi_version_major, 1);
        assert_eq!(info.abi_version_minor, 4);
        assert_eq!(info.abi_version_patch, 0);
        assert!(info.capabilities > 0);
        assert!(
            (info.capabilities & ffi::XBLOB_CAPABILITY_FRAMEBUFFER_PRESENTATION) != 0,
            "Must have FRAMEBUFFER_PRESENTATION capability"
        );
        assert!(
            (info.capabilities & ffi::XBLOB_CAPABILITY_NV2A_GPU) != 0,
            "Must have NV2A_GPU capability"
        );
        assert!(
            (info.capabilities & ffi::XBLOB_CAPABILITY_XDVDFS_VFS) != 0,
            "Must have XDVDFS_VFS capability"
        );
        assert!(
            (info.capabilities & ffi::XBLOB_CAPABILITY_MEDIA_BOOT) != 0,
            "Must have MEDIA_BOOT capability"
        );
        assert_eq!(info.product_name, "xblob");
        assert_eq!(info.product_version, "0.1.0");
    }

    #[test]
    fn test_inspect_media_missing_file() {
        let result =
            commands::inspect_media("/path/to/definitely/non_existent_file.xbe".to_string());
        assert!(result.is_err(), "Non-existent file must return error");

        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_FOUND");
    }

    #[test]
    fn test_inspect_media_corrupt_file() {
        let mut temp_file = NamedTempFile::new().expect("create temp file");
        temp_file
            .write_all(b"NOT_A_VALID_HEADER_DATA_1234567890")
            .expect("write dummy bytes");
        let path = temp_file.path().to_str().unwrap().to_string();

        let result = commands::inspect_media(path);
        assert!(result.is_err(), "Corrupt header file must return error");

        let err = result.unwrap_err();
        assert!(
            err.code == "CORRUPT_MEDIA" || err.code == "UNSUPPORTED_FORMAT",
            "Expected CORRUPT_MEDIA or UNSUPPORTED_FORMAT, got {}",
            err.code
        );
    }

    #[test]
    fn test_safe_machine_session_raii_lifecycle_and_frames() {
        let session = ffi::SafeMachineSession::new();
        assert!(
            session.is_ok(),
            "SafeMachineSession creation should succeed"
        );

        let session = session.unwrap();
        let state = session.get_state();
        assert!(state.is_ok(), "Getting state should succeed");
        assert_eq!(state.unwrap(), ffi::XBLOB_MACHINE_STATE_CREATED);

        let meta = session.get_frame_metadata();
        assert!(meta.is_ok(), "Getting frame metadata should succeed");
        let meta = meta.unwrap();
        assert_eq!(meta.is_valid, 0, "Initial frame is_valid must be 0");
        assert_eq!(meta.sequence_number, 0, "Initial sequence number must be 0");

        let pixels = session.copy_frame_pixels(1024);
        assert!(
            pixels.is_ok(),
            "Copying pixels before any frame should succeed"
        );
        assert!(pixels.unwrap().is_empty(), "Initial pixels should be empty");
        // Dropping session here verifies RAII cleanup doesn't crash or leak
    }

    #[test]
    fn test_prepare_machine_diagnostic_missing_file() {
        let result = commands::prepare_machine_diagnostic(
            "/path/to/definitely/non_existent_file.xbe".to_string(),
        );
        assert!(result.is_err(), "Missing file must return error");
        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_FOUND");
    }

    #[test]
    fn test_diagnostic_frame_snapshot_ineligible() {
        let result =
            commands::get_diagnostic_frame_snapshot("/path/to/commercial_game.xbe".to_string());
        assert!(
            result.is_err(),
            "Non-synthetic media must be ineligible for frame snapshot"
        );
        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_ELIGIBLE");
        assert!(err.message.contains("indisponível"));
    }

    #[test]
    fn test_diagnostic_frame_snapshot_missing_synthetic() {
        let result = commands::get_diagnostic_frame_snapshot(
            "/path/to/synthetic_test_workload.xbe".to_string(),
        );
        assert!(
            result.is_err(),
            "Missing synthetic file must return NOT_FOUND"
        );
        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_FOUND");
    }

    #[test]
    fn test_prepare_media_missing_file() {
        let result = tauri::async_runtime::block_on(commands::prepare_media(
            "/path/to/non_existent.iso".to_string(),
        ));
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_FOUND");
    }

    #[test]
    fn test_browse_media_vfs_missing_file() {
        let result = tauri::async_runtime::block_on(commands::browse_media_vfs(
            "/path/to/non_existent.iso".to_string(),
            "".to_string(),
            0,
            10,
        ));
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert_eq!(err.code, "NOT_FOUND");
    }
}
