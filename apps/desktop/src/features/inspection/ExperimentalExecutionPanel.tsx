import React, { useState, useEffect, useCallback } from 'react';
import { getBridge } from '../../bridge';
import { CompatibilityDiagnostic, ExecutionBudgets, MachineSnapshot } from '../../bridge/types';
import { Alert } from '../../components/Toast';
import { Button } from '../../components/Button';
import { Badge } from '../../components/Badge';

export interface ExperimentalExecutionPanelProps {
  filePath: string;
}

export const ExperimentalExecutionPanel: React.FC<ExperimentalExecutionPanelProps> = ({ filePath }) => {
  const [disclaimerAccepted, setDisclaimerAccepted] = useState<boolean>(false);
  const [snapshot, setSnapshot] = useState<MachineSnapshot | null>(null);
  const [diagnostic, setDiagnostic] = useState<CompatibilityDiagnostic | null>(null);
  const [traceText, setTraceText] = useState<string>('');
  const [isExecuting, setIsExecuting] = useState<boolean>(false);
  const [error, setError] = useState<{ code: string; message: string } | null>(null);

  // Budget states
  const [maxInstructions, setMaxInstructions] = useState<number>(1000000);
  const [maxCycles, setMaxCycles] = useState<number>(10000000);
  const [maxWallTimeMs, setMaxWallTimeMs] = useState<number>(2000);

  const getBudgets = useCallback((): ExecutionBudgets => ({
    maxInstructions,
    maxCycles,
    maxWallTimeMs,
    chunkInstructions: 1000,
  }), [maxInstructions, maxCycles, maxWallTimeMs]);

  const refreshState = useCallback(async () => {
    try {
      const bridge = getBridge();
      const [snap, diag, trace] = await Promise.allSettled([
        bridge.getExecutionSnapshot(),
        bridge.getCompatibilityDiagnostic(),
        bridge.getExecutionTrace(),
      ]);

      if (snap.status === 'fulfilled') {
        setSnapshot(snap.value);
      }
      if (diag.status === 'fulfilled') {
        setDiagnostic(diag.value);
      }
      if (trace.status === 'fulfilled') {
        setTraceText(trace.value);
      }
    } catch {
      // Ignored if no active session
    }
  }, []);

  const handleStart = async () => {
    if (!disclaimerAccepted) {
      setError({
        code: 'DISCLAIMER_REQUIRED',
        message: 'Você deve confirmar o aviso de execução experimental e política clean-room antes de executar.',
      });
      return;
    }
    setIsExecuting(true);
    setError(null);
    try {
      const bridge = getBridge();
      const res = await bridge.startTitleExecution(filePath, getBudgets());
      setSnapshot(res);
      await refreshState();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao iniciar execução experimental';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    } finally {
      setIsExecuting(false);
    }
  };

  const handleResume = async () => {
    setIsExecuting(true);
    setError(null);
    try {
      const bridge = getBridge();
      const res = await bridge.resumeTitleExecution(getBudgets());
      setSnapshot(res);
      await refreshState();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao retomar execução';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    } finally {
      setIsExecuting(false);
    }
  };

  const handleStep = async () => {
    setIsExecuting(true);
    setError(null);
    try {
      const bridge = getBridge();
      const stepBudgets: ExecutionBudgets = {
        maxInstructions: 1,
        maxCycles: 10,
        maxWallTimeMs: 100,
        chunkInstructions: 1,
      };
      const res = await bridge.resumeTitleExecution(stepBudgets);
      setSnapshot(res);
      await refreshState();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao executar passo';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    } finally {
      setIsExecuting(false);
    }
  };

  const handlePause = async () => {
    setError(null);
    try {
      const bridge = getBridge();
      const res = await bridge.pauseTitleExecution();
      setSnapshot(res);
      await refreshState();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao pausar execução';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    }
  };

  const handleStop = async () => {
    setError(null);
    try {
      const bridge = getBridge();
      const res = await bridge.stopTitleExecution();
      setSnapshot(res);
      await refreshState();
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Falha ao parar execução';
      const code = err && typeof err === 'object' && 'code' in err ? String((err as { code: unknown }).code) : 'INTERNAL_ERROR';
      setError({ code, message });
    }
  };

  useEffect(() => {
    refreshState();
  }, [refreshState]);

  const getStateBadgeVariant = (state?: string) => {
    switch (state) {
      case 'Running':
        return 'info';
      case 'Paused':
      case 'Prepared':
        return 'warning';
      case 'Stopped':
        return 'default';
      case 'Faulted':
        return 'danger';
      default:
        return 'default';
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
      {/* 1. Clean-Room Experimental Disclaimer */}
      <div
        style={{
          padding: 'var(--space-4)',
          backgroundColor: 'rgba(255, 170, 0, 0.08)',
          border: '1px solid var(--color-warning)',
          borderRadius: 'var(--radius-md)',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'flex-start', gap: 'var(--space-3)' }}>
          <span style={{ fontSize: '1.5rem', lineHeight: 1 }}>⚠️</span>
          <div style={{ flex: 1 }}>
            <h4 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700, color: 'var(--color-warning)' }}>
              Aviso de Execução Experimental & Política Clean-Room
            </h4>
            <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-secondary)', marginTop: 'var(--space-1)' }}>
              Esta funcionalidade implementa a execução de instruções x86 em modo protegido e serviços de kernel sintético HLE estritamente clean-room.
              A execução ocorre exclusivamente sobre a mídia fornecida localmente pelo usuário. O projeto <strong>xblob</strong> não distribui nem requer
              arquivos de BIOS proprietários, chaves de criptografia de hardware ou binários do SDK original. <em>Não há promessa de jogabilidade comercial,
              taxa de quadros estável ou compatibilidade de títulos.</em>
            </p>
            <div style={{ marginTop: 'var(--space-2)', display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
              <input
                type="checkbox"
                id="disclaimer-checkbox"
                checked={disclaimerAccepted}
                onChange={(e) => setDisclaimerAccepted(e.target.checked)}
                style={{ cursor: 'pointer' }}
              />
              <label
                htmlFor="disclaimer-checkbox"
                style={{ fontSize: 'var(--font-size-xs)', fontWeight: 600, cursor: 'pointer' }}
              >
                Compreendo o caráter experimental deste ambiente clean-room e autorizo a execução local da mídia selecionada.
              </label>
            </div>
          </div>
        </div>
      </div>

      {/* 2. Execution Controls & Budgets */}
      <div
        style={{
          padding: 'var(--space-4)',
          backgroundColor: 'var(--color-bg-surface)',
          borderRadius: 'var(--radius-md)',
          display: 'flex',
          flexDirection: 'column',
          gap: 'var(--space-3)',
        }}
      >
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            flexWrap: 'wrap',
            gap: 'var(--space-2)',
          }}
        >
          <div>
            <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700 }}>
              Controles de Execução de Título
            </h3>
            <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
              Ciclo de vida determinístico com budgets granulares e watchdog cooperativo
            </p>
          </div>

          <div style={{ display: 'flex', gap: 'var(--space-2)', flexWrap: 'wrap' }}>
            <Button
              variant="primary"
              onClick={handleStart}
              disabled={isExecuting || !disclaimerAccepted}
              aria-label="Iniciar execução"
            >
              ▶ Iniciar
            </Button>
            <Button
              variant="secondary"
              onClick={handleResume}
              disabled={isExecuting || !snapshot || snapshot.state === 'Running'}
              aria-label="Retomar execução"
            >
              ⏯ Retomar
            </Button>
            <Button
              variant="secondary"
              onClick={handleStep}
              disabled={isExecuting || !snapshot || snapshot.state === 'Running'}
              aria-label="Passo a passo"
            >
              ⏭ Passo (Step)
            </Button>
            <Button
              variant="secondary"
              onClick={handlePause}
              disabled={isExecuting || !snapshot || snapshot.state !== 'Running'}
              aria-label="Pausar execução"
            >
              ⏸ Pausar
            </Button>
            <Button
              variant="danger"
              onClick={handleStop}
              disabled={!snapshot || snapshot.state === 'Stopped'}
              aria-label="Parar execução"
            >
              ⏹ Parar
            </Button>
          </div>
        </div>

        {/* Budget Inputs */}
        <div
          style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))',
            gap: 'var(--space-3)',
            marginTop: 'var(--space-2)',
            padding: 'var(--space-3)',
            backgroundColor: 'var(--color-bg-canvas)',
            borderRadius: 'var(--radius-md)',
          }}
        >
          <div>
            <label
              htmlFor="budget-instructions"
              style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', display: 'block' }}
            >
              Budget Instruções
            </label>
            <input
              id="budget-instructions"
              type="number"
              value={maxInstructions}
              onChange={(e) => setMaxInstructions(Math.max(1, parseInt(e.target.value, 10) || 1000))}
              style={{
                width: '100%',
                marginTop: 'var(--space-1)',
                padding: 'var(--space-1) var(--space-2)',
                borderRadius: 'var(--radius-sm)',
                border: '1px solid var(--color-border-subtle)',
                backgroundColor: 'var(--color-bg-surface)',
                color: 'var(--color-text-primary)',
              }}
            />
          </div>

          <div>
            <label
              htmlFor="budget-cycles"
              style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', display: 'block' }}
            >
              Budget Ciclos
            </label>
            <input
              id="budget-cycles"
              type="number"
              value={maxCycles}
              onChange={(e) => setMaxCycles(Math.max(10, parseInt(e.target.value, 10) || 10000))}
              style={{
                width: '100%',
                marginTop: 'var(--space-1)',
                padding: 'var(--space-1) var(--space-2)',
                borderRadius: 'var(--radius-sm)',
                border: '1px solid var(--color-border-subtle)',
                backgroundColor: 'var(--color-bg-surface)',
                color: 'var(--color-text-primary)',
              }}
            />
          </div>

          <div>
            <label
              htmlFor="budget-walltime"
              style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', display: 'block' }}
            >
              Limite Tempo Real (ms)
            </label>
            <input
              id="budget-walltime"
              type="number"
              value={maxWallTimeMs}
              onChange={(e) => setMaxWallTimeMs(Math.max(10, parseInt(e.target.value, 10) || 1000))}
              style={{
                width: '100%',
                marginTop: 'var(--space-1)',
                padding: 'var(--space-1) var(--space-2)',
                borderRadius: 'var(--radius-sm)',
                border: '1px solid var(--color-border-subtle)',
                backgroundColor: 'var(--color-bg-surface)',
                color: 'var(--color-text-primary)',
              }}
            />
          </div>
        </div>

        {error && (
          <Alert variant="error" title="Erro de Execução">
            <p>{error.message}</p>
            <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', marginTop: 'var(--space-1)' }}>
              Código: <code>{error.code}</code>
            </p>
          </Alert>
        )}
      </div>

      {/* 3. Snapshot & Live Metrics Display */}
      {snapshot && (
        <div
          style={{
            padding: 'var(--space-4)',
            backgroundColor: 'var(--color-bg-surface)',
            borderRadius: 'var(--radius-md)',
            display: 'flex',
            flexDirection: 'column',
            gap: 'var(--space-3)',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 'var(--space-2)' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
              <Badge variant={getStateBadgeVariant(snapshot.state)}>
                {snapshot.state.toUpperCase()}
              </Badge>
              <Badge variant="default">
                Parada: {snapshot.stopReasonCode}
              </Badge>
              {snapshot.stopReasonSymbol && (
                <Badge variant="warning">
                  {snapshot.stopReasonSymbol}
                </Badge>
              )}
            </div>

            <div style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
              Thread Ativa: {snapshot.activeThreadId} / Total: {snapshot.threadCount}
            </div>
          </div>

          <div
            style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fit, minmax(140px, 1fr))',
              gap: 'var(--space-3)',
            }}
          >
            <div style={{ padding: 'var(--space-2) var(--space-3)', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Instruções Executadas</span>
              <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600 }}>{snapshot.instructionsExecuted.toLocaleString()}</p>
            </div>
            <div style={{ padding: 'var(--space-2) var(--space-3)', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Ciclos Consumidos</span>
              <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600 }}>{snapshot.currentCycle.toLocaleString()}</p>
            </div>
            <div style={{ padding: 'var(--space-2) var(--space-3)', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Eventos Disparados</span>
              <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600 }}>{snapshot.eventsFired.toLocaleString()}</p>
            </div>
            <div style={{ padding: 'var(--space-2) var(--space-3)', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>EIP de Parada</span>
              <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600, fontFamily: 'var(--font-mono)' }}>{snapshot.registers.eipHex}</p>
            </div>
          </div>

          {snapshot.stopReasonDetail && (
            <div
              style={{
                padding: 'var(--space-2) var(--space-3)',
                backgroundColor: 'var(--color-bg-canvas)',
                borderRadius: 'var(--radius-sm)',
                borderLeft: '3px solid var(--color-warning)',
              }}
            >
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', fontWeight: 600 }}>
                Detalhe da Interrupção:
              </span>
              <p style={{ fontSize: 'var(--font-size-sm)', marginTop: '2px' }}>{snapshot.stopReasonDetail}</p>
            </div>
          )}

          {/* CPU Registers Snapshot Table */}
          <div style={{ marginTop: 'var(--space-2)' }}>
            <h4 style={{ fontSize: 'var(--font-size-sm)', fontWeight: 700, marginBottom: 'var(--space-2)' }}>
              Registradores da CPU IA-32 (Snapshot de Estado)
            </h4>
            <div
              style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(110px, 1fr))',
                gap: 'var(--space-2)',
                fontFamily: 'var(--font-mono)',
                fontSize: 'var(--font-size-xs)',
              }}
            >
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EAX: </span>
                <strong>{snapshot.registers.eaxHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>ECX: </span>
                <strong>{snapshot.registers.ecxHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EDX: </span>
                <strong>{snapshot.registers.edxHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EBX: </span>
                <strong>{snapshot.registers.ebxHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>ESP: </span>
                <strong>{snapshot.registers.espHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EBP: </span>
                <strong>{snapshot.registers.ebpHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>ESI: </span>
                <strong>{snapshot.registers.esiHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EDI: </span>
                <strong>{snapshot.registers.ediHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EIP: </span>
                <strong>{snapshot.registers.eipHex}</strong>
              </div>
              <div style={{ padding: '6px 8px', backgroundColor: 'var(--color-bg-canvas)', borderRadius: 'var(--radius-sm)' }}>
                <span style={{ color: 'var(--color-text-muted)' }}>EFLAGS: </span>
                <strong>{snapshot.registers.eflagsHex}</strong>
              </div>
            </div>
          </div>

          {/* Bounded Stack Words Preview */}
          <div style={{ marginTop: 'var(--space-2)' }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 'var(--space-2)' }}>
              <h4 style={{ fontSize: 'var(--font-size-sm)', fontWeight: 700 }}>
                Visualização de Pilha Bounded (ESP Preview)
              </h4>
              <Badge variant={snapshot.stackValid ? 'success' : 'warning'}>
                {snapshot.stackValid ? 'ESP Válido (8 Palavras)' : 'ESP Fora dos Limites de RAM'}
              </Badge>
            </div>

            {snapshot.stackValid && snapshot.stackWordsHex.length > 0 ? (
              <div
                style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fit, minmax(130px, 1fr))',
                  gap: 'var(--space-2)',
                  fontFamily: 'var(--font-mono)',
                  fontSize: 'var(--font-size-xs)',
                }}
              >
                {snapshot.stackWordsHex.map((word, idx) => (
                  <div
                    key={idx}
                    style={{
                      padding: '6px 8px',
                      backgroundColor: 'var(--color-bg-canvas)',
                      borderRadius: 'var(--radius-sm)',
                    }}
                  >
                    <span style={{ color: 'var(--color-text-muted)' }}>+{idx * 4}: </span>
                    <code>{word}</code>
                  </div>
                ))}
              </div>
            ) : (
              <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
                Pilha inacessível no endereço ESP atual.
              </p>
            )}
          </div>
        </div>
      )}

      {/* 4. First Blocker Compatibility Diagnostic Card */}
      {diagnostic && diagnostic.blockerCount > 0 && (
        <div
          style={{
            padding: 'var(--space-4)',
            backgroundColor: 'var(--color-bg-surface)',
            borderRadius: 'var(--radius-md)',
            borderLeft: '4px solid var(--color-danger, #ff4444)',
            display: 'flex',
            flexDirection: 'column',
            gap: 'var(--space-2)',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap' }}>
            <h4 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700, color: 'var(--color-danger, #ff4444)' }}>
              Diagnóstico de Compatibilidade: Primeiro Bloqueador Detectado
            </h4>
            <Badge variant="danger">{diagnostic.firstBlockerCode}</Badge>
          </div>

          <p style={{ fontSize: 'var(--font-size-sm)', marginTop: 'var(--space-1)' }}>
            {diagnostic.blockerDetail}
          </p>

          <dl
            style={{
              display: 'grid',
              gridTemplateColumns: '180px 1fr',
              rowGap: 'var(--space-1)',
              fontSize: 'var(--font-size-xs)',
              marginTop: 'var(--space-2)',
            }}
          >
            <dt style={{ color: 'var(--color-text-muted)' }}>Categoria:</dt>
            <dd><strong>{diagnostic.blockerCategory}</strong></dd>

            <dt style={{ color: 'var(--color-text-muted)' }}>Símbolo / Mnemônico:</dt>
            <dd><code>{diagnostic.blockerSymbolOrMnemonic || '-'}</code></dd>

            <dt style={{ color: 'var(--color-text-muted)' }}>Ordinal / Opcode Hex:</dt>
            <dd><code>{diagnostic.blockerOrdinalOrOpcodeHex} ({diagnostic.blockerOrdinalOrOpcode})</code></dd>

            <dt style={{ color: 'var(--color-text-muted)' }}>EIP da Ocorrência:</dt>
            <dd><code>{diagnostic.blockerEipHex}</code></dd>

            <dt style={{ color: 'var(--color-text-muted)' }}>Thread ID:</dt>
            <dd>{diagnostic.blockerThreadId}</dd>

            <dt style={{ color: 'var(--color-text-muted)' }}>Total Instruções Executadas:</dt>
            <dd>{diagnostic.totalInstructions.toLocaleString()}</dd>
          </dl>
        </div>
      )}

      {/* 5. Execution Trace Viewer */}
      <div
        style={{
          padding: 'var(--space-4)',
          backgroundColor: 'var(--color-bg-surface)',
          borderRadius: 'var(--radius-md)',
          display: 'flex',
          flexDirection: 'column',
          gap: 'var(--space-2)',
        }}
      >
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <h4 style={{ fontSize: 'var(--font-size-sm)', fontWeight: 700 }}>
            Buffer de Rastreamento de Execução Recente (Ring Buffer)
          </h4>
          <Button variant="secondary" size="sm" onClick={refreshState}>
            Atualizar Rastreamento
          </Button>
        </div>

        <pre
          style={{
            backgroundColor: 'var(--color-bg-canvas)',
            color: 'var(--color-text-primary)',
            padding: 'var(--space-3)',
            borderRadius: 'var(--radius-sm)',
            border: '1px solid var(--color-border-subtle)',
            fontSize: 'var(--font-size-xs)',
            maxHeight: '220px',
            overflowY: 'auto',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-all',
            fontFamily: 'var(--font-mono)',
          }}
        >
          {traceText || '(Nenhum evento registrado no ring buffer)'}
        </pre>
      </div>
    </div>
  );
};
