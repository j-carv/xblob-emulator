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
    expect(info.abiVersionMinor).toBe(1);
    expect(info.abiVersionPatch).toBe(0);
    expect(info.capabilities).toBeGreaterThan(0);
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
});
