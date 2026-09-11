import { invoke } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';
import {
  BootReport,
  BridgeApi,
  CoreInfo,
  GpuFrameSnapshot,
  MachinePrepareDiagnostic,
  MediaReport,
  VfsDirectoryPage,
} from './types';

export class TauriBridge implements BridgeApi {
  async getCoreInfo(): Promise<CoreInfo> {
    return await invoke<CoreInfo>('get_core_info');
  }

  async inspectMedia(filePath: string): Promise<MediaReport> {
    return await invoke<MediaReport>('inspect_media', { path: filePath });
  }

  async prepareMachineDiagnostic(filePath: string): Promise<MachinePrepareDiagnostic> {
    return await invoke<MachinePrepareDiagnostic>('prepare_machine_diagnostic', { path: filePath });
  }

  async getDiagnosticFrameSnapshot(filePath: string): Promise<GpuFrameSnapshot> {
    return await invoke<GpuFrameSnapshot>('get_diagnostic_frame_snapshot', { path: filePath });
  }

  async prepareMedia(filePath: string): Promise<BootReport> {
    return await invoke<BootReport>('prepare_media', { path: filePath });
  }

  async browseMediaVfs(
    filePath: string,
    directory: string,
    offset: number,
    limit: number
  ): Promise<VfsDirectoryPage> {
    return await invoke<VfsDirectoryPage>('browse_media_vfs', {
      path: filePath,
      directory,
      offset,
      limit,
    });
  }

  async pickFile(): Promise<string | null> {
    const selected = await open({
      multiple: false,
      directory: false,
      filters: [
        {
          name: 'Arquivos Xbox (*.xbe, *.iso)',
          extensions: ['xbe', 'iso', 'bin'],
        },
        {
          name: 'Todos os arquivos (*.*)',
          extensions: ['*'],
        },
      ],
    });

    if (typeof selected === 'string') {
      return selected;
    }
    return null;
  }
}
