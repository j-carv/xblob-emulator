import { invoke } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';
import { BridgeApi, CoreInfo, MachinePrepareDiagnostic, MediaReport } from './types';

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
