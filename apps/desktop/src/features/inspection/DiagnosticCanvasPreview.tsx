import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getBridge } from '../../bridge';
import { GpuFrameSnapshot } from '../../bridge/types';
import { Alert } from '../../components/Toast';
import { Badge } from '../../components/Badge';
import { Button } from '../../components/Button';

export interface DiagnosticCanvasPreviewProps {
  filePath: string;
  isPrepared: boolean;
  isSyntheticEligible: boolean;
}

export const DiagnosticCanvasPreview: React.FC<DiagnosticCanvasPreviewProps> = ({
  filePath,
  isPrepared,
  isSyntheticEligible,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [snapshot, setSnapshot] = useState<GpuFrameSnapshot | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(false);
  const [error, setError] = useState<{ code: string; message: string } | null>(null);

  const fetchSnapshot = useCallback(async () => {
    if (!isSyntheticEligible || !isPrepared) return;

    setIsLoading(true);
    setError(null);
    try {
      const bridge = getBridge();
      const snap = await bridge.getDiagnosticFrameSnapshot(filePath);
      setSnapshot(snap);
    } catch (err: unknown) {
      let code = 'INTERNAL_ERROR';
      let message = 'Falha ao obter snapshot do framebuffer';
      if (err instanceof Error) {
        message = err.message;
        if ('code' in err && typeof (err as { code: unknown }).code === 'string') {
          code = (err as { code: string }).code;
        }
      }
      setError({ code, message });
      setSnapshot(null);
    } finally {
      setIsLoading(false);
    }
  }, [filePath, isPrepared, isSyntheticEligible]);

  useEffect(() => {
    if (isSyntheticEligible && isPrepared) {
      void fetchSnapshot();
    } else {
      setSnapshot(null);
      setError(null);
    }
  }, [isSyntheticEligible, isPrepared, fetchSnapshot]);

  // Render to canvas with cleanup
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !snapshot || !snapshot.metadata.isValid) {
      return;
    }

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    try {
      const binaryString = atob(snapshot.pixelsBase64);
      const bytes = new Uint8ClampedArray(binaryString.length);
      for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }
      const imgData = new ImageData(bytes, snapshot.metadata.width, snapshot.metadata.height);
      ctx.putImageData(imgData, 0, 0);
    } catch (e) {
      console.error('Erro ao decodificar pixels do framebuffer:', e);
    }

    return () => {
      // Memory cleanup: clear canvas buffer on unmount or update
      if (canvasRef.current) {
        const c = canvasRef.current;
        const cCtx = c.getContext('2d');
        if (cCtx) {
          cCtx.clearRect(0, 0, c.width, c.height);
        }
      }
    };
  }, [snapshot]);

  // 1. Ineligible media state
  if (!isSyntheticEligible) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <Badge variant="warning">Mídia Não Suportada Neste Marco</Badge>
          <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
            Restrito a cargas diagnósticas e sintéticas nesta fase
          </span>
        </div>
        <Alert variant="info" title="Visualização Gráfica Indisponível para Mídia Geral Neste Marco">
          <p style={{ fontSize: 'var(--font-size-sm)', lineHeight: '1.5' }}>
            O subsistema gráfico experimental NV2A aceita nesta fase apenas fixtures de teste e diagnósticos sintéticos clean-room.
            O suporte à execução e renderização de títulos fornecidos pelo usuário está em desenvolvimento e ainda não executa neste marco.
          </p>
        </Alert>
      </div>
    );
  }

  // 2. Not prepared yet state
  if (!isPrepared) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <Badge variant="default">Aguardando Validação</Badge>
        </div>
        <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
          A sessão de máquina convidada precisa ser validada antes de inspecionar o framebuffer.
          Utilize a aba <strong>Validação & Preparação</strong> para inicializar a sessão.
        </p>
      </div>
    );
  }

  // 3. Loading state
  if (isLoading) {
    return (
      <div
        role="status"
        aria-live="polite"
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: 'var(--space-3)',
          padding: 'var(--space-4)',
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
          Obtendo snapshot do framebuffer via IPC/ABI C 1.3...
        </span>
      </div>
    );
  }

  // 4. Error state
  if (error) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
        <Alert variant="error" title="Falha ao Acessar Framebuffer">
          <p>{error.message}</p>
          <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', marginTop: 'var(--space-1)' }}>
            Código: <code>{error.code}</code>
          </p>
        </Alert>
        <div>
          <Button variant="secondary" size="sm" onClick={fetchSnapshot}>
            Tentar Novamente
          </Button>
        </div>
      </div>
    );
  }

  // 5. Empty frame state (no valid flip yet)
  if (!snapshot || !snapshot.metadata.isValid) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <Badge variant="default">Aguardando Flip</Badge>
          <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
            Sequência 0
          </span>
        </div>
        <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
          Nenhum frame diagnóstico foi apresentado ainda. O framebuffer aguarda a submissão de comandos de flip da GPU.
        </p>
        <div>
          <Button variant="secondary" size="sm" onClick={fetchSnapshot}>
            Atualizar Snapshot
          </Button>
        </div>
      </div>
    );
  }

  // 6. Active Frame Render
  const meta = snapshot.metadata;
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <Badge variant="success">Frame Ativo (Seq {meta.sequenceNumber})</Badge>
          <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
            {meta.width} × {meta.height} RGBA8
          </span>
        </div>
        <Button variant="secondary" size="sm" onClick={fetchSnapshot}>
          Atualizar Snapshot
        </Button>
      </div>

      <div
        style={{
          display: 'flex',
          justifyContent: 'center',
          alignItems: 'center',
          backgroundColor: 'var(--color-bg-canvas)',
          borderRadius: 'var(--radius-md)',
          padding: 'var(--space-4)',
          border: '1px solid var(--color-border-subtle)',
        }}
      >
        <canvas
          ref={canvasRef}
          width={meta.width}
          height={meta.height}
          role="img"
          aria-label={`Framebuffer diagnóstico NV2A ${meta.width}x${meta.height}, sequência ${meta.sequenceNumber}`}
          style={{
            maxWidth: '100%',
            height: 'auto',
            aspectRatio: `${meta.width} / ${meta.height}`,
            imageRendering: 'pixelated',
            borderRadius: 'var(--radius-sm)',
            boxShadow: '0 4px 12px rgba(0, 0, 0, 0.25)',
          }}
        />
      </div>

      <dl
        style={{
          display: 'grid',
          gridTemplateColumns: '200px 1fr',
          rowGap: 'var(--space-2)',
          fontSize: 'var(--font-size-sm)',
          backgroundColor: 'var(--color-bg-surface)',
          padding: 'var(--space-3)',
          borderRadius: 'var(--radius-md)',
        }}
      >
        <dt style={{ color: 'var(--color-text-muted)' }}>Resolução:</dt>
        <dd><code>{meta.width} × {meta.height}</code></dd>

        <dt style={{ color: 'var(--color-text-muted)' }}>Pitch da Linha:</dt>
        <dd><code>{meta.pitch} bytes</code></dd>

        <dt style={{ color: 'var(--color-text-muted)' }}>Tamanho do Framebuffer:</dt>
        <dd><code>{meta.bufferSize} bytes</code></dd>

        <dt style={{ color: 'var(--color-text-muted)' }}>Formato de Pixel:</dt>
        <dd><code>RGBA8 Linear (32 bpp)</code></dd>

        <dt style={{ color: 'var(--color-text-muted)' }}>Ciclo do Scheduler:</dt>
        <dd><code>{meta.frameCycle}</code></dd>
      </dl>
    </div>
  );
};
