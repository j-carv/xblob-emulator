import { describe, it, expect } from 'vitest';
import { MockBridge } from '../bridge/mockBridge';
import { createBridge, isTauriEnvironment } from '../bridge';

describe('Bridge layer and DTOs', () => {
  it('should detect browser environment correctly', () => {
    expect(isTauriEnvironment()).toBe(false);
  });

  it('should instantiate MockBridge when forced or outside Tauri', () => {
    const bridge = createBridge(true);
    expect(bridge).toBeInstanceOf(MockBridge);
  });

  it('should fetch CoreInfo matching ABI C structure', async () => {
    const bridge = new MockBridge();
    const info = await bridge.getCoreInfo();

    expect(info.abiVersionMajor).toBe(1);
    expect(info.abiVersionMinor).toBe(4);
    expect(info.abiVersionPatch).toBe(0);
    expect(info.capabilities).toBeGreaterThan(0);
    expect(info.capabilities & (1 << 9)).toBeGreaterThan(0);
    expect(info.capabilities & (1 << 10)).toBeGreaterThan(0);
    expect(info.capabilities & (1 << 11)).toBeGreaterThan(0);
    expect(info.capabilities & (1 << 12)).toBeGreaterThan(0);
    expect(info.productName).toBe('xblob');
    expect(info.productVersion).toBe('0.1.0');
  });

  it('should prepare synthetic XBE machine diagnostics', async () => {
    const bridge = new MockBridge();
    const diag = await bridge.prepareMachineDiagnostic('/test/synthetic/default.xbe');

    expect(diag.state).toBe('Prepared');
    expect(diag.isPrepared).toBe(true);
    expect(diag.entryPointHex).toBe('0x00011000');
    expect(diag.sectionCount).toBe(3);
    expect(diag.ramSizeBytes).toBe(67108864);
  });

  it('should throw typed errors when preparing missing or corrupt files', async () => {
    const bridge = new MockBridge();

    await expect(bridge.prepareMachineDiagnostic('/test/missing_file.xbe')).rejects.toThrow(
      'Arquivo não encontrado no caminho especificado'
    );

    await expect(bridge.prepareMachineDiagnostic('/test/corrupt_file.xbe')).rejects.toThrow(
      'Assinatura mágica ou cabeçalho de imagem inválido'
    );
  });

  it('should inspect synthetic XBE media and return complete DTO', async () => {
    const bridge = new MockBridge();
    const report = await bridge.inspectMedia('/test/game/default.xbe');

    expect(report.type).toBe('xbe');
    expect(report.filePath).toBe('/test/game/default.xbe');
    expect(report.fileSize).toBeGreaterThan(0);
    expect(report.humanSummary).toContain('xblob Relatório de Inspeção de Mídia');
    expect(report.xbe).toBeDefined();
    expect(report.xbe?.titleName).toBe('Halo: Combat Evolved');
    expect(report.xbe?.titleIdHex).toBe('0x4D530004');
    expect(report.xbe?.sectionCount).toBe(3);
  });

  it('should inspect synthetic XISO media and return complete DTO', async () => {
    const bridge = new MockBridge();
    const report = await bridge.inspectMedia('/test/game/game.iso');

    expect(report.type).toBe('xiso_trimmed');
    expect(report.xiso).toBeDefined();
    expect(report.xiso?.variant).toBe('XisoTrimmed');
    expect(report.xiso?.validFooterMagic).toBe(true);
  });

  it('should throw typed errors for missing or corrupt files', async () => {
    const bridge = new MockBridge();

    await expect(bridge.inspectMedia('/test/missing_file.xbe')).rejects.toThrow(
      'Arquivo não encontrado no caminho especificado'
    );

    await expect(bridge.inspectMedia('/test/corrupt_file.xbe')).rejects.toThrow(
      'Assinatura mágica ou cabeçalho de imagem inválido'
    );

    await expect(bridge.inspectMedia('/test/unsupported.txt')).rejects.toThrow(
      'O formato do arquivo não é reconhecido como mídia ou executável Xbox'
    );
  });

  it('should get diagnostic frame snapshot for eligible synthetic media', async () => {
    const bridge = new MockBridge();
    const snapshot = await bridge.getDiagnosticFrameSnapshot('/test/synthetic/test_app.xbe');

    expect(snapshot.metadata.isValid).toBe(true);
    expect(snapshot.metadata.width).toBe(64);
    expect(snapshot.metadata.height).toBe(64);
    expect(snapshot.metadata.pitch).toBe(256);
    expect(snapshot.metadata.bufferSize).toBe(64 * 64 * 4);
    expect(snapshot.pixelsBase64.length).toBeGreaterThan(0);
  });

  it('should reject diagnostic frame snapshot for ineligible commercial media with NOT_ELIGIBLE', async () => {
    const bridge = new MockBridge();

    try {
      await bridge.getDiagnosticFrameSnapshot('/games/halo/default.xbe');
      expect.unreachable('Should have thrown NOT_ELIGIBLE error');
    } catch (err: unknown) {
      expect((err as { code?: string }).code).toBe('NOT_ELIGIBLE');
      expect((err as Error).message).toContain('indisponível');
    }
  });

  it('should throw NOT_FOUND for missing synthetic files when fetching frame', async () => {
    const bridge = new MockBridge();

    try {
      await bridge.getDiagnosticFrameSnapshot('/synthetic/missing_workload.xbe');
      expect.unreachable('Should have thrown NOT_FOUND error');
    } catch (err: unknown) {
      expect((err as { code?: string }).code).toBe('NOT_FOUND');
    }
  });

  it('should prepare media and return BootReport', async () => {
    const bridge = new MockBridge();
    const report = await bridge.prepareMedia('/games/clean_room/game.iso');

    expect(report.isBootable).toBe(true);
    expect(report.mediaType).toBe('xiso_trimmed');
    expect(report.defaultXbePath).toBe('D:\\DEFAULT.XBE');
    expect(report.titleName).toBe('Mock Clean-Room Title');
    expect(report.titleIdHex).toBe('0x12345678');
    expect(report.entryPointHex).toBe('0x00011000');
    expect(report.sectionCount).toBe(3);
    expect(report.mediaSizeBytes).toBeGreaterThan(0);
  });

  it('should throw typed errors when preparing missing or corrupt media', async () => {
    const bridge = new MockBridge();

    await expect(bridge.prepareMedia('/games/missing.iso')).rejects.toThrow(
      'Arquivo não encontrado no caminho especificado'
    );

    await expect(bridge.prepareMedia('/games/corrupt.iso')).rejects.toThrow(
      'Assinatura mágica ou cabeçalho de imagem inválido'
    );

    await expect(bridge.prepareMedia('/games/no_xbe.iso')).rejects.toThrow(
      'Nenhum default.xbe executável encontrado na mídia'
    );
  });

  it('should browse media VFS with pagination and entries', async () => {
    const bridge = new MockBridge();
    const page0 = await bridge.browseMediaVfs('/games/game.iso', '', 0, 5);

    expect(page0.totalCount).toBe(12);
    expect(page0.offset).toBe(0);
    expect(page0.limit).toBe(5);
    expect(page0.entries.length).toBe(5);
    expect(page0.entries[0].name).toBe('DEFAULT.XBE');
    expect(page0.entries[0].isDirectory).toBe(false);

    const page1 = await bridge.browseMediaVfs('/games/game.iso', '', 5, 5);
    expect(page1.entries.length).toBe(5);
    expect(page1.offset).toBe(5);

    await expect(bridge.browseMediaVfs('/games/missing.iso', '', 0, 5)).rejects.toThrow(
      'Arquivo não encontrado no caminho especificado'
    );
  });
});
