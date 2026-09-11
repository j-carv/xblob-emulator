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
            commands::prepare_machine_diagnostic
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
        assert_eq!(info.abi_version_minor, 2);
        assert_eq!(info.abi_version_patch, 0);
        assert!(info.capabilities > 0);
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
    fn test_safe_machine_session_raii_lifecycle() {
        let session = ffi::SafeMachineSession::new();
        assert!(
            session.is_ok(),
            "SafeMachineSession creation should succeed"
        );

        let session = session.unwrap();
        let state = session.get_state();
        assert!(state.is_ok(), "Getting state should succeed");
        assert_eq!(state.unwrap(), ffi::XBLOB_MACHINE_STATE_CREATED);
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
}
