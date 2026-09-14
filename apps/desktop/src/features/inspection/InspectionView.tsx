import React, { useState } from 'react';
import { MachinePrepareDiagnostic, MediaReport, MediaType } from '../../bridge/types';
import { Card } from '../../components/Card';
import { Badge } from '../../components/Badge';
import { Button } from '../../components/Button';
import { Tabs, TabItem } from '../../components/Tabs';
import { Alert } from '../../components/Toast';
import { DiagnosticCanvasPreview } from './DiagnosticCanvasPreview';
import { ExperimentalExecutionPanel } from './ExperimentalExecutionPanel';
import { MediaBootPanel } from './MediaBootPanel';
import { ValidationPanel } from './ValidationPanel';
import { XdvdfsBrowser } from './XdvdfsBrowser';

export interface InspectionViewProps {
  report: MediaReport | null;
  error: { code: string; message: string } | null;
  isLoading: boolean;
  onPickFile: () => void;
  onReset: () => void;
}

export const InspectionView: React.FC<InspectionViewProps> = ({
  report,
  error,
  isLoading,
  onPickFile,
  onReset,
}) => {
  const [activeTab, setActiveTab] = useState<string>('summary');
  const [isDragging, setIsDragging] = useState<boolean>(false);
  const [diagnostic, setDiagnostic] = useState<MachinePrepareDiagnostic | null>(null);

  const formatBytes = (bytes: number): string => {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(2)} KiB`;
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GiB`;
  };

  const getFormatBadge = (type: MediaType) => {
    switch (type) {
      case 'xbe':
        return <Badge variant="success">Executável XBE</Badge>;
      case 'xiso_trimmed':
        return <Badge variant="info">Xbox ISO (Trimmed)</Badge>;
      case 'xiso_raw':
        return <Badge variant="info">Xbox ISO (Raw Redump)</Badge>;
      case 'iso9660_unsupported':
        return <Badge variant="warning">ISO 9660 Padrão (Sem XDVDFS)</Badge>;
      case 'unknown':
      default:
        return <Badge variant="default">Desconhecido</Badge>;
    }
  };

  // 1. Loading State
  if (isLoading) {
    return (
      <Card title="Inspeção em Andamento">
        <div
          role="status"
          aria-live="polite"
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: 'var(--space-10) 0',
            gap: 'var(--space-4)',
          }}
        >
          <div
            aria-hidden="true"
            style={{
              width: '48px',
              height: '48px',
              border: '4px solid var(--color-border-subtle)',
              borderTopColor: 'var(--color-primary)',
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite',
            }}
          />
          <p style={{ fontSize: 'var(--font-size-lg)', fontWeight: 600, color: 'var(--color-text-primary)' }}>
            Lendo e analisando arquivo através da ABI C...
          </p>
          <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
            Extraindo cabeçalhos e metadados com segurança sem execução de código
          </p>
        </div>
      </Card>
    );
  }

  // 2. Error State
  if (error) {
    const getFriendlyErrorDetails = () => {
      switch (error.code) {
        case 'NOT_FOUND':
          return {
            title: 'Arquivo Não Encontrado',
            description: 'O caminho selecionado não pôde ser lido. Verifique se o arquivo ainda existe e se você tem permissão de leitura.',
          };
        case 'UNSUPPORTED_FORMAT':
          return {
            title: 'Formato de Mídia Não Reconhecido',
            description: 'O arquivo informado não possui a assinatura mágica de executáveis Xbox (XBEH) ou sistema de arquivos de disco (XDVDFS). Certifique-se de selecionar um arquivo .xbe ou imagem de disco Xbox.',
          };
        case 'CORRUPT_MEDIA':
          return {
            title: 'Mídia ou Cabeçalho Corrompido',
            description: 'A estrutura interna do arquivo apresenta valores inconsistentes ou truncados que violam a especificação de integridade.',
          };
        default:
          return {
            title: 'Erro na Inspeção',
            description: error.message || 'Ocorreu uma falha ao tentar inspecionar o arquivo fornecido.',
          };
      }
    };

    const details = getFriendlyErrorDetails();

    return (
      <Card title="Resultado da Inspeção">
        <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-6)' }}>
          <Alert variant="error" title={details.title}>
            <p style={{ marginBottom: 'var(--space-2)' }}>{details.description}</p>
            <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
              Código de status: <code>{error.code}</code>
            </p>
          </Alert>

          <div style={{ display: 'flex', gap: 'var(--space-3)' }}>
            <Button variant="primary" onClick={onPickFile}>
              Tentar Outro Arquivo
            </Button>
            <Button variant="secondary" onClick={onReset}>
              Voltar ao Início
            </Button>
          </div>
        </div>
      </Card>
    );
  }

  // 3. Result Inspection Report View
  if (report) {
    const tabs: TabItem[] = [
      {
        id: 'summary',
        label: 'Resumo Geral',
        content: (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
            <div
              style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))',
                gap: 'var(--space-3)',
              }}
            >
              <div style={{ padding: 'var(--space-3)', backgroundColor: 'var(--color-bg-surface)', borderRadius: 'var(--radius-md)' }}>
                <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Tamanho</span>
                <p style={{ fontSize: 'var(--font-size-lg)', fontWeight: 600 }}>{formatBytes(report.fileSize)}</p>
              </div>

              <div style={{ padding: 'var(--space-3)', backgroundColor: 'var(--color-bg-surface)', borderRadius: 'var(--radius-md)' }}>
                <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Formato</span>
                <div style={{ marginTop: 'var(--space-1)' }}>{getFormatBadge(report.type)}</div>
              </div>

              {report.xbe && (
                <div style={{ padding: 'var(--space-3)', backgroundColor: 'var(--color-bg-surface)', borderRadius: 'var(--radius-md)' }}>
                  <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>Total de Seções</span>
                  <p style={{ fontSize: 'var(--font-size-lg)', fontWeight: 600 }}>{report.xbe.sectionCount}</p>
                </div>
              )}
            </div>

            <div style={{ marginTop: 'var(--space-2)' }}>
              <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', textTransform: 'uppercase' }}>
                Caminho do Arquivo
              </span>
              <p
                style={{
                  wordBreak: 'break-all',
                  fontFamily: 'var(--font-mono)',
                  fontSize: 'var(--font-size-sm)',
                  backgroundColor: 'var(--color-bg-surface)',
                  padding: 'var(--space-2) var(--space-3)',
                  borderRadius: 'var(--radius-md)',
                  border: '1px solid var(--color-border-subtle)',
                  marginTop: 'var(--space-1)',
                }}
              >
                {report.filePath}
              </p>
            </div>
          </div>
        ),
      },
    ];

    if (report.xbe) {
      tabs.push({
        id: 'xbe',
        label: 'Executável XBE',
        content: (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
            <div style={{ padding: 'var(--space-4)', backgroundColor: 'var(--color-bg-surface)', borderRadius: 'var(--radius-md)' }}>
              <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700, marginBottom: 'var(--space-2)' }}>
                Certificado do Título
              </h3>
              <dl style={{ display: 'grid', gridTemplateColumns: '160px 1fr', rowGap: 'var(--space-2)', fontSize: 'var(--font-size-sm)' }}>
                <dt style={{ color: 'var(--color-text-muted)' }}>Nome do Título:</dt>
                <dd style={{ fontWeight: 600 }}>{report.xbe.titleName || '(Não informado)'}</dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Title ID:</dt>
                <dd><code>{report.xbe.titleIdHex || '0x00000000'}</code></dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Número do Disco:</dt>
                <dd>{report.xbe.diskNumber ?? '-'}</dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Região:</dt>
                <dd>{report.xbe.gameRegion !== undefined ? `0x${report.xbe.gameRegion.toString(16).padStart(8, '0')}` : '-'}</dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Ponto de Entrada:</dt>
                <dd><code>{report.xbe.entryPointHex || '-'}</code></dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Número de Seções:</dt>
                <dd>{report.xbe.sectionCount}</dd>
              </dl>
            </div>
          </div>
        ),
      });

      tabs.push({
        id: 'validation',
        label: 'Validação & Preparação',
        content: (
          <ValidationPanel
            filePath={report.filePath}
            onDiagnosticChange={setDiagnostic}
          />
        ),
      });

      tabs.push({
        id: 'display',
        label: 'Framebuffer NV2A',
        content: (
          <DiagnosticCanvasPreview
            filePath={report.filePath}
            isPrepared={diagnostic?.isPrepared ?? false}
            isSyntheticEligible={
              report.filePath.toLowerCase().includes('synthetic') ||
              report.filePath.toLowerCase().includes('diagnostic') ||
              (report.xbe?.titleName.toLowerCase().includes('synthetic') ?? false)
            }
          />
        ),
      });
    }

    if (report.xiso) {
      tabs.push({
        id: 'xiso',
        label: 'Sistema XDVDFS',
        content: (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-3)' }}>
            <div style={{ padding: 'var(--space-4)', backgroundColor: 'var(--color-bg-surface)', borderRadius: 'var(--radius-md)' }}>
              <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700, marginBottom: 'var(--space-2)' }}>
                Metadados do Contêiner ISO
              </h3>
              <dl style={{ display: 'grid', gridTemplateColumns: '200px 1fr', rowGap: 'var(--space-2)', fontSize: 'var(--font-size-sm)' }}>
                <dt style={{ color: 'var(--color-text-muted)' }}>Variante Detectada:</dt>
                <dd style={{ fontWeight: 600 }}>{report.xiso.variant}</dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Offset do Descritor:</dt>
                <dd><code>0x{(report.xiso.volumeDescriptorOffset ?? 0).toString(16).toUpperCase()}</code></dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Setor do Diretório Raiz:</dt>
                <dd><code>0x{(report.xiso.rootDirSector ?? 0).toString(16).toUpperCase()}</code></dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Tamanho do Diretório Raiz:</dt>
                <dd>{report.xiso.rootDirSize ?? 0} bytes</dd>

                <dt style={{ color: 'var(--color-text-muted)' }}>Assinatura de Rodapé:</dt>
                <dd>{report.xiso.validFooterMagic ? 'Válida (MICROSOFT*XBOX*MEDIA)' : 'Ausente / Inválida'}</dd>
              </dl>
            </div>
          </div>
        ),
      });
    }

    if (report.xiso || report.type === 'xiso_trimmed' || report.type === 'xiso_raw') {
      tabs.push({
        id: 'xdvdfs_browser',
        label: 'Navegador XDVDFS',
        content: <XdvdfsBrowser filePath={report.filePath} />,
      });
    }

    if (report.type === 'xbe' || report.type === 'xiso_trimmed' || report.type === 'xiso_raw') {
      tabs.push({
        id: 'media_boot',
        label: 'Preparar Mídia',
        content: <MediaBootPanel filePath={report.filePath} />,
      });

      tabs.push({
        id: 'execution',
        label: '⚡ Execução Experimental',
        content: <ExperimentalExecutionPanel filePath={report.filePath} />,
      });
    }

    tabs.push({
      id: 'raw',
      label: 'Relatório Completo',
      content: (
        <pre
          style={{
            backgroundColor: 'var(--color-bg-canvas)',
            color: 'var(--color-text-primary)',
            padding: 'var(--space-4)',
            borderRadius: 'var(--radius-md)',
            border: '1px solid var(--color-border-subtle)',
            fontSize: 'var(--font-size-xs)',
            overflowX: 'auto',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-all',
          }}
        >
          {report.humanSummary}
        </pre>
      ),
    });

    return (
      <Card
        title={report.xbe?.titleName || 'Relatório de Inspeção de Mídia'}
        subtitle={report.filePath}
        headerAction={
          <Button variant="secondary" size="sm" onClick={onPickFile}>
            📂 Abrir Outro
          </Button>
        }
      >
        <Tabs tabs={tabs} activeTab={activeTab} onTabChange={setActiveTab} />
      </Card>
    );
  }

  // 4. Initial Empty State
  return (
    <Card
      title="Inspecionar Mídia Xbox"
      subtitle="Selecione um arquivo de executável (.xbe) ou imagem de disco (.iso, .bin)"
    >
      <div
        onDragOver={(e) => {
          e.preventDefault();
          setIsDragging(true);
        }}
        onDragLeave={() => setIsDragging(false)}
        onDrop={(e) => {
          e.preventDefault();
          setIsDragging(false);
          onPickFile();
        }}
        style={{
          border: `2px dashed ${isDragging ? 'var(--color-primary)' : 'var(--color-border-subtle)'}`,
          backgroundColor: isDragging ? 'var(--color-bg-surface-hover)' : 'var(--color-bg-surface)',
          borderRadius: 'var(--radius-lg)',
          padding: 'var(--space-10) var(--space-6)',
          textAlign: 'center',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: 'var(--space-4)',
          transition: 'all var(--transition-fast)',
        }}
      >
        <span aria-hidden="true" style={{ fontSize: '3rem' }}>
          💿
        </span>
        <div>
          <p style={{ fontSize: 'var(--font-size-lg)', fontWeight: 600, color: 'var(--color-text-primary)' }}>
            Selecione uma mídia para inspecionar
          </p>
          <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)', marginTop: 'var(--space-1)' }}>
            Suporte a executáveis XBE e imagens de disco XDVDFS (trimmed ou raw)
          </p>
        </div>

        <Button variant="primary" size="lg" onClick={onPickFile}>
          Selecionar Arquivo do Disco...
        </Button>
      </div>
    </Card>
  );
};
