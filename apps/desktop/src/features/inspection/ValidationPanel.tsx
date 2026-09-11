import React, { useState } from 'react';
import { getBridge } from '../../bridge';
import { MachinePrepareDiagnostic } from '../../bridge/types';
import { Alert } from '../../components/Toast';
import { Button } from '../../components/Button';
import { Badge } from '../../components/Badge';

export interface ValidationPanelProps {
  filePath: string;
  onDiagnosticChange?: (diag: MachinePrepareDiagnostic | null) => void;
}

export const ValidationPanel: React.FC<ValidationPanelProps> = ({
  filePath,
  onDiagnosticChange,
}) => {
  const [diagnostic, setDiagnostic] = useState<MachinePrepareDiagnostic | null>(null);
  const [isPreparing, setIsPreparing] = useState<boolean>(false);
  const [preparationError, setPreparationError] = useState<{
    code: string;
    message: string;
  } | null>(null);

  const handlePrepareDiagnostic = async () => {
    setIsPreparing(true);
    setPreparationError(null);
    try {
      const bridge = getBridge();
      const result = await bridge.prepareMachineDiagnostic(filePath);
      setDiagnostic(result);
      onDiagnosticChange?.(result);
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao preparar sessão';
      const code =
        err && typeof err === 'object' && 'code' in err
          ? String((err as { code: unknown }).code)
          : 'INTERNAL_ERROR';
      setPreparationError({ code, message });
      setDiagnostic(null);
      onDiagnosticChange?.(null);
    } finally {
      setIsPreparing(false);
    }
  };

  const formatBytes = (bytes: number): string => {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(2)} KiB`;
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GiB`;
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
      <div
        style={{
          padding: 'var(--space-4)',
          backgroundColor: 'var(--color-bg-surface)',
          borderRadius: 'var(--radius-md)',
        }}
      >
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: 'var(--space-3)',
          }}
        >
          <div>
            <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700 }}>
              Sessão Diagnóstica de Memória e Loader
            </h3>
            <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
              Validação em duas fases e paginação virtual IA-32 (4 KiB)
            </p>
          </div>
          <Button
            variant="primary"
            onClick={handlePrepareDiagnostic}
            disabled={isPreparing}
          >
            {isPreparing ? 'Preparando...' : 'Validar e Preparar Sessão'}
          </Button>
        </div>

        {isPreparing && (
          <div
            role="status"
            aria-live="polite"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 'var(--space-3)',
              padding: 'var(--space-3)',
              backgroundColor: 'var(--color-bg-canvas)',
              borderRadius: 'var(--radius-md)',
            }}
          >
            <div
              aria-hidden="true"
              style={{
                width: '20px',
                height: '20px',
                border: '2px solid var(--color-border-subtle)',
                borderTopColor: 'var(--color-primary)',
                borderRadius: '50%',
                animation: 'spin 0.8s linear infinite',
              }}
            />
            <span style={{ fontSize: 'var(--font-size-sm)' }}>
              Verificando cabeçalhos XBE e mapeando páginas de memória virtual...
            </span>
          </div>
        )}

        {preparationError && (
          <Alert variant="error" title="Falha na Preparação da Sessão">
            <p>{preparationError.message}</p>
            <p
              style={{
                fontSize: 'var(--font-size-xs)',
                color: 'var(--color-text-muted)',
                marginTop: 'var(--space-1)',
              }}
            >
              Código de status: <code>{preparationError.code}</code>
            </p>
          </Alert>
        )}

        {diagnostic && !isPreparing && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
              <Badge variant={diagnostic.isPrepared ? 'success' : 'warning'}>
                {diagnostic.isPrepared
                  ? `Sessão Pronta (${diagnostic.state})`
                  : `Estado: ${diagnostic.state}`}
              </Badge>
            </div>

            <dl
              style={{
                display: 'grid',
                gridTemplateColumns: '220px 1fr',
                rowGap: 'var(--space-2)',
                fontSize: 'var(--font-size-sm)',
              }}
            >
              <dt style={{ color: 'var(--color-text-muted)' }}>Ponto de Entrada:</dt>
              <dd>
                <code>{diagnostic.entryPointHex}</code>
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Seções Mapeadas:</dt>
              <dd>{diagnostic.sectionCount}</dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Tamanho de Cabeçalhos:</dt>
              <dd>{formatBytes(diagnostic.headersSize)}</dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Tamanho da Imagem em RAM:</dt>
              <dd>{formatBytes(diagnostic.imageSize)}</dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Memória Física Convidado:</dt>
              <dd>
                <code>0x00000000 - 0x03FFFFFF (64 MiB Retail)</code>
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Barramento Convidado (MMIO):</dt>
              <dd>
                <code>0xFD000000 (Synthetic PCI/MMIO)</code>
              </dd>
            </dl>
          </div>
        )}
      </div>

      <Alert variant="info" title="Escopo e Limitações Arquiteturais">
        <p style={{ fontSize: 'var(--font-size-xs)', lineHeight: '1.5' }}>
          Este ambiente opera em modo diagnóstico para validação estática de formatos e
          estruturas de memória. Nenhum componente proprietário (BIOS, chaves ou SDK
          comercial) é distribuído no projeto. Ações interativas de execução de jogos
          (como controles Play/Run) ainda não estão disponíveis neste marco, aguardando
          a validação completa do pipeline funcional de execução.
        </p>
      </Alert>
    </div>
  );
};
