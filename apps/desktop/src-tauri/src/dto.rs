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
pub struct GpuFrameMetadataDto {
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub pixel_format: u32,
    pub sequence_number: u64,
    pub frame_cycle: u64,
    pub buffer_size: u32,
    pub is_valid: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct BootReportDto {
    pub media_type: String,
    pub default_xbe_path: String,
    pub title_name: String,
    pub title_id: u32,
    pub title_id_hex: String,
    pub entry_point: u32,
    pub entry_point_hex: String,
    pub section_count: u32,
    pub media_size_bytes: u64,
    pub is_bootable: bool,
    pub error_message: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct VfsEntryDto {
    pub name: String,
    pub size: u64,
    pub is_directory: bool,
    pub attributes: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct VfsDirectoryPageDto {
    pub total_count: u32,
    pub offset: u32,
    pub limit: u32,
    pub entries: Vec<VfsEntryDto>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct GpuFrameSnapshotDto {
    pub metadata: GpuFrameMetadataDto,
    pub pixels_base64: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AppErrorDto {
    pub code: String,
    pub message: String,
    pub details: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ExecutionBudgetsDto {
    pub max_instructions: Option<u64>,
    pub max_cycles: Option<u64>,
    pub max_wall_time_ms: Option<u64>,
    pub max_events: Option<u64>,
    pub chunk_instructions: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CpuRegistersDto {
    pub eax: u32,
    pub eax_hex: String,
    pub ecx: u32,
    pub ecx_hex: String,
    pub edx: u32,
    pub edx_hex: String,
    pub ebx: u32,
    pub ebx_hex: String,
    pub esp: u32,
    pub esp_hex: String,
    pub ebp: u32,
    pub ebp_hex: String,
    pub esi: u32,
    pub esi_hex: String,
    pub edi: u32,
    pub edi_hex: String,
    pub eip: u32,
    pub eip_hex: String,
    pub eflags: u32,
    pub eflags_hex: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct MachineSnapshotDto {
    pub state: String,
    pub stop_reason_code: String,
    pub fault_eip: u32,
    pub fault_eip_hex: String,
    pub active_thread_id: u32,
    pub thread_count: u32,
    pub current_cycle: u64,
    pub instructions_executed: u64,
    pub events_fired: u64,
    pub registers: CpuRegistersDto,
    pub stack_valid: bool,
    pub stack_words: Vec<u32>,
    pub stack_words_hex: Vec<String>,
    pub stop_reason_category: String,
    pub stop_reason_symbol: String,
    pub stop_reason_detail: String,
    pub error_message: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CompatibilityDiagnosticDto {
    pub first_blocker_code: String,
    pub blocker_ordinal_or_opcode: u32,
    pub blocker_ordinal_or_opcode_hex: String,
    pub blocker_thread_id: u32,
    pub blocker_eip: u32,
    pub blocker_eip_hex: String,
    pub blocker_count: u64,
    pub blocker_category: String,
    pub blocker_symbol_or_mnemonic: String,
    pub blocker_detail: String,
    pub total_instructions: u64,
    pub total_cycles: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct HostInputSnapshotDto {
    pub sequence: u64,
    pub connected: bool,
    pub digital_buttons: u8,
    pub button_a: u8,
    pub button_b: u8,
    pub button_x: u8,
    pub button_y: u8,
    pub button_black: u8,
    pub button_white: u8,
    pub trigger_left: u8,
    pub trigger_right: u8,
    pub thumb_lx: i16,
    pub thumb_ly: i16,
    pub thumb_rx: i16,
    pub thumb_ry: i16,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct InteractiveMetricsDto {
    pub frame_sequence: u64,
    pub input_sequence: u64,
    pub instructions_executed: u64,
    pub cycles_consumed: u64,
    pub unsupported_gpu_count: u32,
    pub unsupported_usb_count: u32,
    pub state: String,
    pub stop_reason: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RumbleStateDto {
    pub left_motor: u16,
    pub right_motor: u16,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct UnsupportedFeatureEntryDto {
    pub subsystem: String,
    pub capability: String,
    pub identifier: u32,
    pub identifier_hex: String,
    pub count: u64,
    pub first_context: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct InteractiveFrameDto {
    pub frame_sequence: u64,
    pub input_sequence: u64,
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub pixel_format: u32,
    pub has_new_frame: bool,
    pub pixels_base64: Option<String>,
    pub rumble: RumbleStateDto,
    pub metrics: InteractiveMetricsDto,
}

impl std::fmt::Display for AppErrorDto {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.code, self.message)
    }
}

impl std::error::Error for AppErrorDto {}
