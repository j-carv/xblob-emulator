import React, { useState } from 'react';
import { getBridge } from '../../bridge';
import { BootReport } from '../../bridge/types';
import { Alert } from '../../components/Toast';
import { Button } from '../../components/Button';
import { Badge } from '../../components/Badge';

export interface MediaBootPanelProps {
  filePath: string;
}

export const MediaBootPanel: React.FC<MediaBootPanelProps> = ({ filePath }) => {
  const [bootReport, setBootReport] = useState<BootReport | null>(null);
  const [isPreparing, setIsPreparing] = useState<boolean>(false);
  const [error, setError] = useState<{ code: string; message: string } | null>(null);

  const handlePrepareMedia = async () => {
    setIsPreparing(true);
    setError(null);
    try {
      const bridge = getBridge();
      const res = await bridge.prepareMedia(filePath);
      setBootReport(res);
    } catch (err: unknown) {
      const message =
        err instanceof Error ? err.message : 'Falha ao preparar mídia';
      const code =
        err && typeof err === 'object' && 'code' in err
          ? String((err as { code: unknown }).code)
          : 'INTERNAL_ERROR';
      setError({ code, message });
      setBootReport(null);
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
            flexWrap: 'wrap',
            gap: 'var(--space-2)',
          }}
        >
          <div>
            <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700 }}>
              Preparação de Mídia e Montagem VFS
            </h3>
            <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
              Monta o sistema de arquivos virtual em modo somente-leitura e localiza default.xbe
            </p>
          </div>

          <Button
            variant="primary"
            onClick={handlePrepareMedia}
            disabled={isPreparing}
            aria-label="Preparar mídia"
          >
            {isPreparing ? 'Preparando Mídia...' : 'Preparar mídia'}
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
              marginTop: 'var(--space-2)',
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
              Montando volume XDVDFS, resolvendo default.xbe e estruturando sessão...
            </span>
          </div>
        )}

        {error && (
          <Alert variant="error" title="Falha ao Preparar Mídia">
            <p>{error.message}</p>
            <p
              style={{
                fontSize: 'var(--font-size-xs)',
                color: 'var(--color-text-muted)',
                marginTop: 'var(--space-1)',
              }}
            >
              Código de status: <code>{error.code}</code>
            </p>
          </Alert>
        )}

        {bootReport && !isPreparing && (
          <div
            style={{
              display: 'flex',
              flexDirection: 'column',
              gap: 'var(--space-3)',
              marginTop: 'var(--space-2)',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
              <Badge variant={bootReport.isBootable ? 'success' : 'warning'}>
                {bootReport.isBootable ? 'Mídia Pronta (Prepared)' : 'Não Inicializável'}
              </Badge>
              <Badge variant="info">VFS: D:\ Montado (Read-Only)</Badge>
            </div>

            <dl
              style={{
                display: 'grid',
                gridTemplateColumns: '220px 1fr',
                rowGap: 'var(--space-2)',
                fontSize: 'var(--font-size-sm)',
              }}
            >
              <dt style={{ color: 'var(--color-text-muted)' }}>Executável Resolvido:</dt>
              <dd>
                <code>{bootReport.defaultXbePath}</code>
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Título:</dt>
              <dd style={{ fontWeight: 600 }}>
                {bootReport.titleName || '(Sem nome registrado)'}
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Title ID:</dt>
              <dd>
                <code>{bootReport.titleIdHex}</code>
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Ponto de Entrada:</dt>
              <dd>
                <code>{bootReport.entryPointHex}</code>
              </dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Seções Mapeadas:</dt>
              <dd>{bootReport.sectionCount}</dd>

              <dt style={{ color: 'var(--color-text-muted)' }}>Tamanho da Mídia:</dt>
              <dd>{formatBytes(bootReport.mediaSizeBytes)}</dd>
            </dl>
          </div>
        )}
      </div>

      <Alert variant="info" title="Aviso Importante: Preparação Distinta de Execução">
        <p style={{ fontSize: 'var(--font-size-xs)', lineHeight: '1.5' }}>
          A ação <strong>&quot;Preparar mídia&quot;</strong> valida os cabeçalhos, monta o volume
          XDVDFS em modo somente-leitura e estrutura a sessão de memória do convidado. Esta ação é
          estritamente distinta de executar ou jogar (Play): a execução interativa e gameplay
          ainda não estão implementados nem disponíveis neste marco do emulador.
        </p>
      </Alert>
    </div>
  );
};
