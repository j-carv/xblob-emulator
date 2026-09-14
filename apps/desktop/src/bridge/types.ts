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

export interface ExecutionBudgets {
  maxInstructions?: number;
  maxCycles?: number;
  maxWallTimeMs?: number;
  maxEvents?: number;
  chunkInstructions?: number;
}

export interface CpuRegisters {
  eax: number;
  eaxHex: string;
  ecx: number;
  ecxHex: string;
  edx: number;
  edxHex: string;
  ebx: number;
  ebxHex: string;
  esp: number;
  espHex: string;
  ebp: number;
  ebpHex: string;
  esi: number;
  esiHex: string;
  edi: number;
  ediHex: string;
  eip: number;
  eipHex: string;
  eflags: number;
  eflagsHex: string;
}

export interface MachineSnapshot {
  state: string;
  stopReasonCode: string;
  faultEip: number;
  faultEipHex: string;
  activeThreadId: number;
  threadCount: number;
  currentCycle: number;
  instructionsExecuted: number;
  eventsFired: number;
  registers: CpuRegisters;
  stackValid: boolean;
  stackWords: number[];
  stackWordsHex: string[];
  stopReasonCategory: string;
  stopReasonSymbol: string;
  stopReasonDetail: string;
  errorMessage?: string;
}

export interface CompatibilityDiagnostic {
  firstBlockerCode: string;
  blockerOrdinalOrOpcode: number;
  blockerOrdinalOrOpcodeHex: string;
  blockerThreadId: number;
  blockerEip: number;
  blockerEipHex: string;
  blockerCount: number;
  blockerCategory: string;
  blockerSymbolOrMnemonic: string;
  blockerDetail: string;
  totalInstructions: number;
  totalCycles: number;
}

export interface BridgeApi {
  getCoreInfo(): Promise<CoreInfo>;
  inspectMedia(filePath: string): Promise<MediaReport>;
  prepareMachineDiagnostic(filePath: string): Promise<MachinePrepareDiagnostic>;
  getDiagnosticFrameSnapshot(filePath: string): Promise<GpuFrameSnapshot>;
  prepareMedia(filePath: string): Promise<BootReport>;
  browseMediaVfs(filePath: string, directory: string, offset: number, limit: number): Promise<VfsDirectoryPage>;
  pickFile(): Promise<string | null>;
  startTitleExecution(filePath: string, budgets?: ExecutionBudgets): Promise<MachineSnapshot>;
  resumeTitleExecution(budgets?: ExecutionBudgets): Promise<MachineSnapshot>;
  pauseTitleExecution(): Promise<MachineSnapshot>;
  stopTitleExecution(): Promise<MachineSnapshot>;
  getExecutionSnapshot(): Promise<MachineSnapshot>;
  getCompatibilityDiagnostic(): Promise<CompatibilityDiagnostic>;
  getExecutionTrace(): Promise<string>;
}
