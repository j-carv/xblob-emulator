import { BridgeApi, CoreInfo, MachinePrepareDiagnostic, MediaReport } from './types';

export class MockBridge implements BridgeApi {
  private mockFilePickerSequence = 0;

  async getCoreInfo(): Promise<CoreInfo> {
    await new Promise((r) => setTimeout(r, 50));
    return {
      abiVersionMajor: 1,
      abiVersionMinor: 1,
      abiVersionPatch: 0,
      capabilities: 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
      productName: 'xblob',
      productVersion: '0.1.0',
    };
  }

  async inspectMedia(filePath: string): Promise<MediaReport> {
    await new Promise((r) => setTimeout(r, 200));

    if (filePath.includes('missing') || filePath.includes('notfound')) {
      const err = new Error('Arquivo não encontrado no caminho especificado');
      (err as unknown as { code: string }).code = 'NOT_FOUND';
      throw err;
    }

    if (filePath.includes('corrupt') || filePath.includes('invalid_magic')) {
      const err = new Error('Assinatura mágica ou cabeçalho de imagem inválido');
      (err as unknown as { code: string }).code = 'CORRUPT_MEDIA';
      throw err;
    }

    if (filePath.includes('unsupported') || filePath.endsWith('.txt')) {
      const err = new Error('O formato do arquivo não é reconhecido como mídia ou executável Xbox');
      (err as unknown as { code: string }).code = 'UNSUPPORTED_FORMAT';
      throw err;
    }

    if (filePath.endsWith('.iso')) {
      return {
        type: 'xiso_trimmed',
        filePath,
        fileSize: 4700372992,
        humanSummary:
          `========================================================\n` +
          ` xblob Relatório de Inspeção de Mídia\n` +
          `========================================================\n` +
          `Arquivo:             ${filePath}\n` +
          `Tamanho:             4700372992 bytes\n` +
          `Formato detectado:   Xbox ISO (XDVDFS Trimmed)\n\n` +
          `--- Metadados do Sistema de Arquivos XDVDFS ---\n` +
          `Variante:            XisoTrimmed\n` +
          `Offset do Descritor: 0x10000 (65536 bytes)\n` +
          `Setor raiz dir:      0x00000200 (offset 0x100000)\n` +
          `Tamanho raiz dir:    4096 bytes\n` +
          `Assinatura rodapé:   Válida (MICROSOFT*XBOX*MEDIA)\n` +
          `========================================================`,
        xiso: {
          variant: 'XisoTrimmed',
          volumeDescriptorOffset: 0x10000,
          rootDirSector: 0x200,
          rootDirSize: 4096,
          validFooterMagic: true,
        },
      };
    }

    // Default to XBE
    return {
      type: 'xbe',
      filePath,
      fileSize: 3145728,
      humanSummary:
        `========================================================\n` +
        ` xblob Relatório de Inspeção de Mídia\n` +
        `========================================================\n` +
        `Arquivo:             ${filePath}\n` +
        `Tamanho:             3145728 bytes\n` +
        `Formato detectado:   Executável Xbox (XBE)\n\n` +
        `--- Metadados do Cabeçalho XBE ---\n` +
        `Endereço base:       0x00010000\n` +
        `Tamanho cabeçalhos:  0x1000 (4096 bytes)\n` +
        `Tamanho da imagem:   0x00300000 (3145728 bytes)\n` +
        `Timestamp:           0x3e18a520\n` +
        `Ponto de entrada:    0x00011000\n` +
        `Total de seções:     3\n\n` +
        `--- Certificado do Título ---\n` +
        `Title ID:            0x4d530004\n` +
        `Nome do título:      "Halo: Combat Evolved"\n` +
        `Versão:              1\n` +
        `Região do jogo:      0x00000001\n` +
        `Número do disco:     1\n\n` +
        `--- Seções ---\n` +
        ` [0] .text | VAddr: 0x00011000 (tam: 0x001a0000) | Raw: 0x1000 (tam: 0x001a0000)\n` +
        ` [1] .data | VAddr: 0x001b1000 (tam: 0x00040000) | Raw: 0x1a1000 (tam: 0x00040000)\n` +
        ` [2] .rsrc | VAddr: 0x001f1000 (tam: 0x00010000) | Raw: 0x1e1000 (tam: 0x00010000)\n` +
        `========================================================`,
      xbe: {
        titleName: 'Halo: Combat Evolved',
        titleId: 0x4d530004,
        titleIdHex: '0x4D530004',
        diskNumber: 1,
        gameRegion: 1,
        entryPoint: 0x00011000,
        entryPointHex: '0x00011000',
        sectionCount: 3,
      },
    };
  }

  async pickFile(): Promise<string | null> {
    await new Promise((r) => setTimeout(r, 100));
    const samples = [
      '/games/halo/default.xbe',
      '/images/jet_set_radio_future.iso',
      '/home/user/games/unsupported_file.txt',
    ];
    const chosen = samples[this.mockFilePickerSequence % samples.length];
    this.mockFilePickerSequence++;
    return chosen;
  }

  async prepareMachineDiagnostic(filePath: string): Promise<MachinePrepareDiagnostic> {
    await new Promise((r) => setTimeout(r, 100));

    if (filePath.includes('missing') || filePath.includes('notfound')) {
      const err = new Error('Arquivo não encontrado no caminho especificado');
      (err as unknown as { code: string }).code = 'NOT_FOUND';
      throw err;
    }

    if (filePath.includes('corrupt') || filePath.includes('invalid_magic') || filePath.endsWith('.txt')) {
      const err = new Error('Assinatura mágica ou cabeçalho de imagem inválido');
      (err as unknown as { code: string }).code = 'CORRUPT_MEDIA';
      throw err;
    }

    return {
      state: 'Prepared',
      entryPoint: 0x00011000,
      entryPointHex: '0x00011000',
      sectionCount: 3,
      headersSize: 4096,
      imageSize: 3145728,
      titleName: 'Synthetic Title',
      titleId: 0x12345678,
      titleIdHex: '0x12345678',
      ramSizeBytes: 67108864,
      isPrepared: true,
    };
  }
}
