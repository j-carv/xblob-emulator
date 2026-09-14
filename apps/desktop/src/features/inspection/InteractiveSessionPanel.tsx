import React, { useState, useEffect, useRef, useCallback } from 'react';
import { getBridge } from '../../bridge';
import { InteractiveFrame, UnsupportedFeatureEntry } from '../../bridge/types';
import { Badge } from '../../components/Badge';
import { Button } from '../../components/Button';
import { Alert } from '../../components/Toast';
import { buildHostInputSnapshot } from './inputMapping';

export interface InteractiveSessionPanelProps {
  filePath: string;
}

export const InteractiveSessionPanel: React.FC<InteractiveSessionPanelProps> = ({ filePath }) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);
  const animFrameIdRef = useRef<number | null>(null);
  const inputSeqRef = useRef<number>(1);
  const lastFrameSeqRef = useRef<number>(0);
  const pressedKeysRef = useRef<Set<string>>(new Set());
  const loopActiveRef = useRef<boolean>(false);

  const [disclaimerAccepted, setDisclaimerAccepted] = useState<boolean>(false);
  const [isRunning, setIsRunning] = useState<boolean>(false);
  const [isFocused, setIsFocused] = useState<boolean>(false);
  const [frameData, setFrameData] = useState<InteractiveFrame | null>(null);
  const [unsupportedList, setUnsupportedList] = useState<UnsupportedFeatureEntry[]>([]);
  const [error, setError] = useState<{ code: string; message: string } | null>(null);

  const drawPixelsToCanvas = useCallback((pixelsBase64: string, width: number, height: number) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    try {
      const binaryString = atob(pixelsBase64);
      const bytes = new Uint8ClampedArray(binaryString.length);
      for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }
      if (canvas.width !== width) canvas.width = width;
      if (canvas.height !== height) canvas.height = height;

      if (bytes.length >= width * height * 4) {
        const imgData = new ImageData(bytes.slice(0, width * height * 4), width, height);
        ctx.putImageData(imgData, 0, 0);
      }
    } catch {
      // Ignored non-fatal decode errors
    }
  }, []);

  const pollInteractiveLoop = useCallback(async () => {
    if (!isRunning) return;

    const bridge = getBridge();
    try {
      // 1. Sample input and submit snapshot
      const gamepads = typeof navigator.getGamepads === 'function' ? navigator.getGamepads() : [];
      const primaryGamepad = gamepads.length > 0 ? gamepads[0] : null;

      const seq = ++inputSeqRef.current;
      const snap = buildHostInputSnapshot(seq, pressedKeysRef.current, primaryGamepad);
      await bridge.submitHostInput(snap);

      // 2. Poll frame backpressure
      const frame = await bridge.getInteractiveFrame(lastFrameSeqRef.current, true);
      setFrameData(frame);

      if (frame.hasNewFrame) {
        lastFrameSeqRef.current = frame.frameSequence;
        if (frame.pixelsBase64) {
          drawPixelsToCanvas(frame.pixelsBase64, frame.width || 640, frame.height || 480);
        }
      }

      // Check if machine halted or faulted
      if (frame.metrics.state === 'Paused' || frame.metrics.state === 'Faulted') {
        setIsRunning(false);
      }
    } catch (e) {
      // Non-fatal frame loop error
      console.warn('Erro transitório no loop interativo:', e);
    }

    if (loopActiveRef.current) {
      animFrameIdRef.current = requestAnimationFrame(() => {
        void pollInteractiveLoop();
      });
    }
  }, [isRunning, drawPixelsToCanvas]);

  useEffect(() => {
    loopActiveRef.current = isRunning;
    if (isRunning) {
      animFrameIdRef.current = requestAnimationFrame(() => {
        void pollInteractiveLoop();
      });
    } else if (animFrameIdRef.current !== null) {
      cancelAnimationFrame(animFrameIdRef.current);
      animFrameIdRef.current = null;
    }

    return () => {
      loopActiveRef.current = false;
      if (animFrameIdRef.current !== null) {
        cancelAnimationFrame(animFrameIdRef.current);
        animFrameIdRef.current = null;
      }
      pressedKeysRef.current.clear();
    };
  }, [isRunning, pollInteractiveLoop]);

  const loadUnsupported = useCallback(async () => {
    try {
      const bridge = getBridge();
      const list = await bridge.getUnsupportedFeatures(0, 50);
      setUnsupportedList(list);
    } catch {
      // Session not yet started
    }
  }, []);

  const handleStart = async () => {
    if (!disclaimerAccepted) {
      setError({
        code: 'DISCLAIMER_REQUIRED',
        message: 'Você deve aceitar o aviso de compatibilidade e política clean-room antes de iniciar.',
      });
      return;
    }
    setError(null);
    try {
      const bridge = getBridge();
      await bridge.startTitleExecution(filePath, {
        maxInstructions: 100000000,
        maxCycles: 500000000,
        maxWallTimeMs: 0,
      });
      setIsRunning(true);
      await loadUnsupported();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao iniciar sessão interativa';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    }
  };

  const handlePause = async () => {
    try {
      const bridge = getBridge();
      await bridge.pauseTitleExecution();
      setIsRunning(false);
      await loadUnsupported();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao pausar sessão';
      setError({ code: 'PAUSE_ERROR', message });
    }
  };

  const handleResume = async () => {
    try {
      const bridge = getBridge();
      await bridge.resumeTitleExecution({
        maxInstructions: 100000000,
        maxCycles: 500000000,
        maxWallTimeMs: 0,
      });
      setIsRunning(true);
      await loadUnsupported();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao retomar sessão';
      setError({ code: 'RESUME_ERROR', message });
    }
  };

  const handleStop = async () => {
    setIsRunning(false);
    try {
      const bridge = getBridge();
      await bridge.stopTitleExecution();
      await loadUnsupported();
    } catch {
      // Ignored
    }
  };

  const handleStep = async () => {
    try {
      const bridge = getBridge();
      await bridge.stepTitleExecution(100);
      const frame = await bridge.getInteractiveFrame(lastFrameSeqRef.current, true);
      setFrameData(frame);
      if (frame.hasNewFrame && frame.pixelsBase64) {
        lastFrameSeqRef.current = frame.frameSequence;
        drawPixelsToCanvas(frame.pixelsBase64, frame.width, frame.height);
      }
      await loadUnsupported();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao executar passo';
      setError({ code: 'STEP_ERROR', message });
    }
  };

  // Keyboard events
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (!isFocused) return;
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) {
        e.preventDefault();
      }
      pressedKeysRef.current.add(e.code);
    };

    const handleKeyUp = (e: KeyboardEvent) => {
      pressedKeysRef.current.delete(e.code);
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, [isFocused]);

  return (
    <section
      ref={containerRef}
      role="region"
      aria-label="Sessão Interativa de Emulação"
      style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}
    >
      {error && (
        <Alert variant="error" title={error.code} onClose={() => setError(null)}>
          {error.message}
        </Alert>
      )}

      {/* Disclaimer */}
      <div
        style={{
          padding: 'var(--space-3)',
          backgroundColor: 'var(--color-bg-subtle)',
          borderRadius: 'var(--radius-md)',
          border: '1px solid var(--color-border-subtle)',
          fontSize: 'var(--font-size-xs)',
        }}
      >
        <label style={{ display: 'flex', alignItems: 'flex-start', gap: 'var(--space-2)', cursor: 'pointer' }}>
          <input
            type="checkbox"
            checked={disclaimerAccepted}
            onChange={(e) => setDisclaimerAccepted(e.target.checked)}
            style={{ marginTop: '2px' }}
          />
          <span>
            <strong>Aviso de Execução Interativa Clean-Room:</strong> Esta emulação é orientada exclusivamente pela especificação de hardware e APIs da plataforma original. Não contém hacks, patches ou atalhos para títulos comerciais. Apenas fixtures e testes sintéticos são mantidos no repositório.
          </span>
        </label>
      </div>

      {/* Controls Bar */}
      <div
        role="toolbar"
        aria-label="Controles da Sessão Interativa"
        style={{ display: 'flex', flexWrap: 'wrap', gap: 'var(--space-2)', alignItems: 'center' }}
      >
        {!isRunning ? (
          <Button variant="primary" size="sm" onClick={handleStart} disabled={!disclaimerAccepted}>
            ▶️ Iniciar Interativo
          </Button>
        ) : (
          <Button variant="secondary" size="sm" onClick={handlePause}>
            ⏸️ Pausar
          </Button>
        )}

        <Button variant="secondary" size="sm" onClick={handleResume} disabled={isRunning || !disclaimerAccepted}>
          ⏯️ Retomar
        </Button>

        <Button variant="secondary" size="sm" onClick={handleStep} disabled={isRunning || !disclaimerAccepted}>
          ⏭️ Passo Único
        </Button>

        <Button variant="danger" size="sm" onClick={handleStop}>
          ⏹️ Parar
        </Button>

        <Button
          variant="secondary"
          size="sm"
          onClick={() => canvasRef.current?.focus()}
          aria-label="Focar janela gráfica para captura de controles"
        >
          🎮 Capturar Entrada
        </Button>

        <Badge variant={isFocused ? 'success' : 'default'}>
          {isFocused ? 'Entrada Capturada (Teclado/Gamepad Ativo)' : 'Sem Foco (Clique no canvas)'}
        </Badge>
      </div>

      {/* Interactive Frame Canvas */}
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          backgroundColor: 'var(--color-bg-canvas)',
          padding: 'var(--space-3)',
          borderRadius: 'var(--radius-md)',
          border: isFocused ? '2px solid var(--color-primary)' : '1px solid var(--color-border-subtle)',
          outline: 'none',
        }}
      >
        <canvas
          ref={canvasRef}
          tabIndex={0}
          role="img"
          aria-label="Quadro Gráfico da Sessão Interativa NV2A"
          width={640}
          height={480}
          onFocus={() => setIsFocused(true)}
          onBlur={() => setIsFocused(false)}
          style={{
            maxWidth: '100%',
            height: 'auto',
            aspectRatio: '4 / 3',
            backgroundColor: '#05070a',
            borderRadius: 'var(--radius-sm)',
            imageRendering: 'pixelated',
            cursor: 'crosshair',
          }}
        />

        <div style={{ display: 'flex', justifyContent: 'space-between', width: '100%', marginTop: 'var(--space-2)', fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
          <span>Resolução: {frameData?.width || 640}x{frameData?.height || 480} RGBA8</span>
          <span>Frame Seq: {frameData?.frameSequence ?? 0}</span>
          <span>Input Seq: {frameData?.inputSequence ?? 0}</span>
          <span>Vibração: L {frameData?.rumble.leftMotor ?? 0} | R {frameData?.rumble.rightMotor ?? 0}</span>
        </div>
      </div>

      {/* Input Mapping Keyboard Hints */}
      <div
        style={{
          padding: 'var(--space-3)',
          backgroundColor: 'var(--color-bg-surface)',
          borderRadius: 'var(--radius-md)',
          fontSize: 'var(--font-size-xs)',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
          gap: 'var(--space-2)',
        }}
      >
        <div><strong>D-Pad:</strong> W/A/S/D ou Setas</div>
        <div><strong>Botões:</strong> Espaço (A), Esc (B), U (X), I (Y)</div>
        <div><strong>Preto / Branco:</strong> E / Q</div>
        <div><strong>Gatilhos LT/RT:</strong> Shift / Ctrl</div>
        <div><strong>Menu / Back:</strong> Enter / Backspace</div>
        <div><strong>Gamepad USB:</strong> Reconhecimento automático</div>
      </div>

      {/* Telemetry & Unsupported Capabilities Table */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-2)' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <h3 style={{ fontSize: 'var(--font-size-sm)', fontWeight: 700 }}>
            Diagnóstico de Compatibilidade Geral (Métodos GPU e Requests USB Não Implementados)
          </h3>
          <Button variant="secondary" size="sm" onClick={loadUnsupported}>
            🔄 Atualizar Diagnóstico
          </Button>
        </div>

        {unsupportedList.length === 0 ? (
          <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
            Nenhuma chamada ou método não suportado registrado até o momento.
          </p>
        ) : (
          <div style={{ overflowX: 'auto', border: '1px solid var(--color-border-subtle)', borderRadius: 'var(--radius-sm)' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 'var(--font-size-xs)' }}>
              <thead>
                <tr style={{ backgroundColor: 'var(--color-bg-subtle)', textAlign: 'left' }}>
                  <th style={{ padding: 'var(--space-2)' }}>Subsistema</th>
                  <th style={{ padding: 'var(--space-2)' }}>Capacidade / Método</th>
                  <th style={{ padding: 'var(--space-2)' }}>Identificador</th>
                  <th style={{ padding: 'var(--space-2)' }}>Ocorrências</th>
                  <th style={{ padding: 'var(--space-2)' }}>Contexto Inicial</th>
                </tr>
              </thead>
              <tbody>
                {unsupportedList.map((entry, idx) => (
                  <tr key={`${entry.subsystem}-${entry.identifier}-${idx}`} style={{ borderTop: '1px solid var(--color-border-subtle)' }}>
                    <td style={{ padding: 'var(--space-2)' }}>
                      <Badge variant={entry.subsystem === 'GPU' ? 'info' : 'warning'}>{entry.subsystem}</Badge>
                    </td>
                    <td style={{ padding: 'var(--space-2)' }}>{entry.capability}</td>
                    <td style={{ padding: 'var(--space-2)' }}><code>{entry.identifierHex}</code></td>
                    <td style={{ padding: 'var(--space-2)' }}>{entry.count}</td>
                    <td style={{ padding: 'var(--space-2)', fontFamily: 'monospace' }}>{entry.firstContext}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </section>
  );
};
