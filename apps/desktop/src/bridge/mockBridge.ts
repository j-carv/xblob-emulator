import {
  BootReport,
  BridgeApi,
  CompatibilityDiagnostic,
  CoreInfo,
  ExecutionBudgets,
  GpuFrameSnapshot,
  MachinePrepareDiagnostic,
  MachineSnapshot,
  MediaReport,
  VfsDirectoryPage,
  VfsEntry,
} from './types';

export class MockBridge implements BridgeApi {
  private mockFilePickerSequence = 0;
  private mockMachineState: 'Created' | 'Prepared' | 'Running' | 'Paused' | 'Stopped' = 'Prepared';
  private mockInstructions = 420;
  private mockCycles = 1260;

  async getCoreInfo(): Promise<CoreInfo> {
    await new Promise((r) => setTimeout(r, 50));
    return {
      abiVersionMajor: 1,
      abiVersionMinor: 5,
      abiVersionPatch: 0,
      capabilities:
        1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 14),
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

  async getDiagnosticFrameSnapshot(filePath: string): Promise<GpuFrameSnapshot> {
    await new Promise((r) => setTimeout(r, 60));

    if (filePath.includes('missing') || filePath.includes('notfound')) {
      const err = new Error('Arquivo não encontrado no caminho especificado');
      (err as unknown as { code: string }).code = 'NOT_FOUND';
      throw err;
    }

    const lower = filePath.toLowerCase();
    if (!lower.includes('synthetic') && !lower.includes('test_') && !lower.includes('diagnostic')) {
      const err = new Error(
        'Visualização gráfica de framebuffer indisponível para esta mídia: apenas fixtures sintéticas clean-room são suportadas.'
      );
      (err as unknown as { code: string }).code = 'NOT_ELIGIBLE';
      throw err;
    }

    // Generate 64x64 synthetic diagnostic test frame
    const width = 64;
    const height = 64;
    const pitch = width * 4;
    const bufferSize = pitch * height;
    const raw = new Uint8Array(bufferSize);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const offset = (y * width + x) * 4;
        raw[offset + 0] = (x * 4) & 0xff; // R
        raw[offset + 1] = 0xcc;           // G
        raw[offset + 2] = (y * 4) & 0xff; // B
        raw[offset + 3] = 0xff;           // A
      }
    }
    let binary = '';
    for (let i = 0; i < raw.length; i++) {
      binary += String.fromCharCode(raw[i]);
    }
    const pixelsBase64 =
      typeof btoa === 'function'
        ? btoa(binary)
        : Buffer.from(binary, 'binary').toString('base64');

    return {
      metadata: {
        width,
        height,
        pitch,
        pixelFormat: 1,
        sequenceNumber: 1,
        frameCycle: 12000,
        bufferSize,
        isValid: true,
      },
      pixelsBase64,
    };
  }

  async prepareMedia(filePath: string): Promise<BootReport> {
    await new Promise((r) => setTimeout(r, 100));

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

    if (filePath.includes('no_xbe')) {
      const err = new Error('Nenhum default.xbe executável encontrado na mídia');
      (err as unknown as { code: string }).code = 'UNSUPPORTED_FORMAT';
      throw err;
    }

    const isIso = filePath.toLowerCase().endsWith('.iso');
    return {
      mediaType: isIso ? 'xiso_trimmed' : 'xbe',
      defaultXbePath: isIso ? 'D:\\DEFAULT.XBE' : 'DEFAULT.XBE',
      titleName: 'Mock Clean-Room Title',
      titleId: 0x12345678,
      titleIdHex: '0x12345678',
      entryPoint: 0x00011000,
      entryPointHex: '0x00011000',
      sectionCount: 3,
      mediaSizeBytes: isIso ? 4700372992 : 3145728,
      isBootable: true,
    };
  }

  async browseMediaVfs(
    filePath: string,
    _directory: string,
    offset: number,
    limit: number
  ): Promise<VfsDirectoryPage> {
    await new Promise((r) => setTimeout(r, 60));

    if (filePath.includes('missing') || filePath.includes('notfound')) {
      const err = new Error('Arquivo não encontrado no caminho especificado');
      (err as unknown as { code: string }).code = 'NOT_FOUND';
      throw err;
    }

    const allEntries: VfsEntry[] = [
      { name: 'DEFAULT.XBE', size: 3145728, isDirectory: false, attributes: 0x80 },
      { name: 'MEDIA', size: 0, isDirectory: true, attributes: 0x10 },
      { name: 'SYSTEM', size: 0, isDirectory: true, attributes: 0x10 },
      { name: 'CONFIG.INI', size: 1024, isDirectory: false, attributes: 0x80 },
      { name: 'SPLASH.BMP', size: 65536, isDirectory: false, attributes: 0x80 },
      { name: 'AUDIO', size: 0, isDirectory: true, attributes: 0x10 },
      { name: 'SHADERS', size: 0, isDirectory: true, attributes: 0x10 },
      { name: 'FONTS', size: 0, isDirectory: true, attributes: 0x10 },
      { name: 'README.TXT', size: 2048, isDirectory: false, attributes: 0x80 },
      { name: 'DATA.BIN', size: 1048576, isDirectory: false, attributes: 0x80 },
      { name: 'PATCH.XBE', size: 524288, isDirectory: false, attributes: 0x80 },
      { name: 'EXTRA', size: 0, isDirectory: true, attributes: 0x10 },
    ];

    const totalCount = allEntries.length;
    const pageEntries = allEntries.slice(offset, offset + limit);

    return {
      totalCount,
      offset,
      limit,
      entries: pageEntries,
    };
  }

  private buildMockSnapshot(state: string, stopReason: string): MachineSnapshot {
    return {
      state,
      stopReasonCode: stopReason,
      faultEip: 0x00012340,
      faultEipHex: '0x00012340',
      activeThreadId: 1,
      threadCount: 1,
      currentCycle: this.mockCycles,
      instructionsExecuted: this.mockInstructions,
      eventsFired: 12,
      registers: {
        eax: 0x00000000,
        eaxHex: '0x00000000',
        ecx: 0x00010000,
        ecxHex: '0x00010000',
        edx: 0x00000042,
        edxHex: '0x00000042',
        ebx: 0x00020000,
        ebxHex: '0x00020000',
        esp: 0x03ffef00,
        espHex: '0x03FFEF00',
        ebp: 0x03ffef20,
        ebpHex: '0x03FFEF20',
        esi: 0x00000000,
        esiHex: '0x00000000',
        edi: 0x00000000,
        ediHex: '0x00000000',
        eip: 0x00011040,
        eipHex: '0x00011040',
        eflags: 0x00000246,
        eflagsHex: '0x00000246',
      },
      stackValid: true,
      stackWords: [0x00011000, 0x00000001, 0x03ffef40, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000],
      stackWordsHex: ['0x00011000', '0x00000001', '0x03FFEF40', '0x00000000', '0x00000000', '0x00000000', '0x00000000', '0x00000000'],
      stopReasonCategory: 'UnsupportedExport',
      stopReasonSymbol: 'AvSetDisplayMode',
      stopReasonDetail: 'Kernel export ordinal 6 (AvSetDisplayMode) is not implemented in synthetic kernel HLE.',
    };
  }

  async startTitleExecution(
    _filePath: string,
    _budgets?: ExecutionBudgets
  ): Promise<MachineSnapshot> {
    await new Promise((r) => setTimeout(r, 100));
    this.mockMachineState = 'Paused';
    this.mockInstructions += 150;
    this.mockCycles += 450;
    return this.buildMockSnapshot('Paused', 'BudgetInstructions');
  }

  async resumeTitleExecution(_budgets?: ExecutionBudgets): Promise<MachineSnapshot> {
    await new Promise((r) => setTimeout(r, 80));
    this.mockMachineState = 'Paused';
    this.mockInstructions += 200;
    this.mockCycles += 600;
    return this.buildMockSnapshot('Paused', 'BudgetInstructions');
  }

  async pauseTitleExecution(): Promise<MachineSnapshot> {
    await new Promise((r) => setTimeout(r, 50));
    this.mockMachineState = 'Paused';
    return this.buildMockSnapshot('Paused', 'Paused');
  }

  async stopTitleExecution(): Promise<MachineSnapshot> {
    await new Promise((r) => setTimeout(r, 50));
    this.mockMachineState = 'Stopped';
    return this.buildMockSnapshot('Stopped', 'Halted');
  }

  async getExecutionSnapshot(): Promise<MachineSnapshot> {
    await new Promise((r) => setTimeout(r, 30));
    return this.buildMockSnapshot(this.mockMachineState, 'None');
  }

  async getCompatibilityDiagnostic(): Promise<CompatibilityDiagnostic> {
    await new Promise((r) => setTimeout(r, 40));
    return {
      firstBlockerCode: 'UnsupportedExport',
      blockerOrdinalOrOpcode: 6,
      blockerOrdinalOrOpcodeHex: '0x00000006',
      blockerThreadId: 1,
      blockerEip: 0x00012340,
      blockerEipHex: '0x00012340',
      blockerCount: 1,
      blockerCategory: 'Kernel Export',
      blockerSymbolOrMnemonic: 'AvSetDisplayMode',
      blockerDetail: 'Ordinal 6 (AvSetDisplayMode) reached without synthetic kernel handler.',
      totalInstructions: this.mockInstructions,
      totalCycles: this.mockCycles,
    };
  }

  async getExecutionTrace(): Promise<string> {
    await new Promise((r) => setTimeout(r, 40));
    return (
      `[00000000] CPU Reset -> EIP: 0x00011000\n` +
      `[00000120] Thread 1 Created -> Entry: 0x00011000 Stack: 0x03FFEF00\n` +
      `[00000450] Kernel Thunk Invoke -> Ordinal 6 (AvSetDisplayMode)\n` +
      `[00000451] Kernel Unsupported Ordinal -> Stopped with UnsupportedExport`
    );
  }
}
