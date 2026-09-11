/**
 * Structural DTOs synchronized with xblob Core C ABI (libs/c_api/include/xblob/c_api.h).
 */

export interface CoreInfo {
  abiVersionMajor: number;
  abiVersionMinor: number;
  abiVersionPatch: number;
  capabilities: number;
  productName: string;
  productVersion: string;
}

export type MediaType =
  | 'unknown'
  | 'xbe'
  | 'xiso_trimmed'
  | 'xiso_raw'
  | 'iso9660_unsupported';

export interface XbeMetadata {
  titleName: string;
  titleId?: number;
  titleIdHex?: string;
  diskNumber?: number;
  gameRegion?: number;
  entryPoint?: number;
  entryPointHex?: string;
  sectionCount: number;
}

export interface XisoMetadata {
  variant: string;
  volumeDescriptorOffset?: number;
  rootDirSector?: number;
  rootDirSize?: number;
  validFooterMagic?: boolean;
}

export interface MediaReport {
  type: MediaType;
  filePath: string;
  fileSize: number;
  humanSummary: string;
  xbe?: XbeMetadata;
  xiso?: XisoMetadata;
}

export interface MachinePrepareDiagnostic {
  state: string;
  entryPoint: number;
  entryPointHex: string;
  sectionCount: number;
  headersSize: number;
  imageSize: number;
  titleName: string;
  titleId: number;
  titleIdHex: string;
  ramSizeBytes: number;
  isPrepared: boolean;
  errorMessage?: string;
}

export interface GpuFrameMetadata {
  width: number;
  height: number;
  pitch: number;
  pixelFormat: number;
  sequenceNumber: number;
  frameCycle: number;
  bufferSize: number;
  isValid: boolean;
}

export interface GpuFrameSnapshot {
  metadata: GpuFrameMetadata;
  pixelsBase64: string;
}

export interface BootReport {
  mediaType: string;
  defaultXbePath: string;
  titleName: string;
  titleId: number;
  titleIdHex: string;
  entryPoint: number;
  entryPointHex: string;
  sectionCount: number;
  mediaSizeBytes: number;
  isBootable: boolean;
  errorMessage?: string;
}

export interface VfsEntry {
  name: string;
  size: number;
  isDirectory: boolean;
  attributes: number;
}

export interface VfsDirectoryPage {
  totalCount: number;
  offset: number;
  limit: number;
  entries: VfsEntry[];
}

export type AppErrorCode =
  | 'INVALID_ARGUMENT'
  | 'NOT_FOUND'
  | 'UNSUPPORTED_FORMAT'
  | 'CORRUPT_MEDIA'
  | 'IO_ERROR'
  | 'INCOMPATIBLE_VERSION'
  | 'INVALID_STATE'
  | 'NOT_ELIGIBLE'
  | 'INTERNAL_ERROR'
  | 'UNKNOWN_ERROR';

export interface AppError {
  code: AppErrorCode;
  message: string;
  details?: string;
}

export interface BridgeApi {
  getCoreInfo(): Promise<CoreInfo>;
  inspectMedia(filePath: string): Promise<MediaReport>;
  prepareMachineDiagnostic(filePath: string): Promise<MachinePrepareDiagnostic>;
  getDiagnosticFrameSnapshot(filePath: string): Promise<GpuFrameSnapshot>;
  prepareMedia(filePath: string): Promise<BootReport>;
  browseMediaVfs(filePath: string, directory: string, offset: number, limit: number): Promise<VfsDirectoryPage>;
  pickFile(): Promise<string | null>;
}
