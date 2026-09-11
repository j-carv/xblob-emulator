use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CoreInfoDto {
    pub abi_version_major: u32,
    pub abi_version_minor: u32,
    pub abi_version_patch: u32,
    pub capabilities: u64,
    pub product_name: String,
    pub product_version: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MediaTypeDto {
    Unknown,
    Xbe,
    XisoTrimmed,
    XisoRaw,
    Iso9660Unsupported,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct XbeMetadataDto {
    pub title_name: String,
    pub title_id: Option<u32>,
    pub title_id_hex: Option<String>,
    pub disk_number: Option<u32>,
    pub game_region: Option<u32>,
    pub entry_point: Option<u32>,
    pub entry_point_hex: Option<String>,
    pub section_count: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct XisoMetadataDto {
    pub variant: String,
    pub volume_descriptor_offset: Option<u64>,
    pub root_dir_sector: Option<u32>,
    pub root_dir_size: Option<u32>,
    pub valid_footer_magic: Option<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct MediaReportDto {
    #[serde(rename = "type")]
    pub media_type: MediaTypeDto,
    pub file_path: String,
    pub file_size: u64,
    pub human_summary: String,
    pub xbe: Option<XbeMetadataDto>,
    pub xiso: Option<XisoMetadataDto>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct MachinePrepareDiagnosticDto {
    pub state: String,
    pub entry_point: u32,
    pub entry_point_hex: String,
    pub section_count: u32,
    pub headers_size: u32,
    pub image_size: u32,
    pub title_name: String,
    pub title_id: u32,
    pub title_id_hex: String,
    pub ram_size_bytes: u64,
    pub is_prepared: bool,
    pub error_message: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AppErrorDto {
    pub code: String,
    pub message: String,
    pub details: Option<String>,
}

impl std::fmt::Display for AppErrorDto {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.code, self.message)
    }
}

impl std::error::Error for AppErrorDto {}
