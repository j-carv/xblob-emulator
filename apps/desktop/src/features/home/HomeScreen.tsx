import React from 'react';
import { CoreInfo } from '../../bridge/types';
import { Card } from '../../components/Card';
import { Badge } from '../../components/Badge';
import { Button } from '../../components/Button';

export interface HomeScreenProps {
  coreInfo: CoreInfo | null;
  isLoadingCoreInfo: boolean;
  onStartInspection: () => void;
  onOpenAbout: () => void;
}

export const HomeScreen: React.FC<HomeScreenProps> = ({
  coreInfo,
  isLoadingCoreInfo,
  onStartInspection,
  onOpenAbout,
}) => {
  const renderCapabilities = (caps: number) => {
    const list: { label: string; active: boolean; desc: string }[] = [
      {
        label: 'Inspeção de Mídia',
        active: (caps & 1) !== 0,
        desc: 'Parser seguro de executáveis XBE e imagens XISO/XDVDFS',
      },
      {
        label: 'Escalonador Determinístico',
        active: (caps & 2) !== 0,
        desc: 'Fila de eventos com resolução exata por ciclos de relógio',
      },
      {
        label: 'Espaço de Memória',
        active: (caps & 4) !== 0,
        desc: 'Gerenciamento de RAM e barramento MMIO de 4 GiB',
      },
      {
        label: 'CPU Sintética',
        active: (caps & 8) !== 0,
        desc: 'Interpretador isolado com registradores x86 estruturados',
      },
    ];

    return (
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(240px, 1fr))',
          gap: 'var(--space-3)',
          marginTop: 'var(--space-2)',
        }}
      >
        {list.map((item) => (
          <div
            key={item.label}
            style={{
              padding: 'var(--space-3)',
              backgroundColor: 'var(--color-bg-surface)',
              borderRadius: 'var(--radius-md)',
              border: '1px solid var(--color-border-subtle)',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 'var(--space-1)' }}>
              <strong style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-primary)' }}>
                {item.label}
              </strong>
              <Badge variant={item.active ? 'success' : 'default'}>
                {item.active ? 'Ativo' : 'Inativo'}
              </Badge>
            </div>
            <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)' }}>
              {item.desc}
            </p>
          </div>
        ))}
      </div>
    );
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-6)', maxWidth: '900px', margin: '0 auto', width: '100%' }}>
      {/* Hero Welcome Section */}
      <section
        aria-labelledby="hero-heading"
        style={{
          padding: 'var(--space-8) var(--space-6)',
          backgroundColor: 'var(--color-bg-card)',
          borderRadius: 'var(--radius-lg)',
          border: '1px solid var(--color-border-subtle)',
          boxShadow: 'var(--shadow-md)',
          textAlign: 'center',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: 'var(--space-4)',
        }}
      >
        <div style={{ display: 'inline-flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <Badge variant="info">Arquitetura de Preservação</Badge>
          <Badge variant="success">ABI C v1.0 Estável</Badge>
        </div>

        <h1
          id="hero-heading"
          style={{
            fontSize: 'var(--font-size-3xl)',
            fontWeight: 800,
            color: 'var(--color-text-primary)',
            letterSpacing: '-0.02em',
          }}
        >
          xblob Desktop
        </h1>

        <p
          style={{
            fontSize: 'var(--font-size-lg)',
            color: 'var(--color-text-secondary)',
            maxWidth: '640px',
            lineHeight: 'var(--line-height-relaxed)',
          }}
        >
          Ambiente modular e de engenharia limpa para inspeção, análise estática e verificação de integridade de mídia e executáveis do Xbox original (6ª geração).
        </p>

        <div
          style={{
            display: 'flex',
            flexWrap: 'wrap',
            gap: 'var(--space-3)',
            justifyContent: 'center',
            marginTop: 'var(--space-2)',
          }}
        >
          <Button variant="primary" size="lg" onClick={onStartInspection}>
            📂 Inspecionar Mídia (.xbe / .iso)
          </Button>
          <Button variant="secondary" size="lg" onClick={onOpenAbout}>
            ℹ️ Sobre e Licenças
          </Button>
        </div>
      </section>

      {/* Core Status & Capabilities */}
      <Card
        title="Status do Núcleo Nativo"
        subtitle="Verificação da ABI C e capacidades dos subsistemas carregados em memória"
      >
        {isLoadingCoreInfo ? (
          <p style={{ color: 'var(--color-text-muted)' }}>Carregando informações do núcleo C++...</p>
        ) : coreInfo ? (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
            <div
              style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
                gap: 'var(--space-4)',
                paddingBottom: 'var(--space-4)',
                borderBottom: '1px solid var(--color-border-subtle)',
              }}
            >
              <div>
                <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', textTransform: 'uppercase' }}>
                  Produto / Versão
                </span>
                <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600, color: 'var(--color-text-primary)' }}>
                  {coreInfo.productName} v{coreInfo.productVersion}
                </p>
              </div>

              <div>
                <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', textTransform: 'uppercase' }}>
                  Versão da ABI C
                </span>
                <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600, color: 'var(--color-text-primary)' }}>
                  v{coreInfo.abiVersionMajor}.{coreInfo.abiVersionMinor}.{coreInfo.abiVersionPatch}
                </p>
              </div>

              <div>
                <span style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', textTransform: 'uppercase' }}>
                  Status da Interface
                </span>
                <p style={{ fontSize: 'var(--font-size-base)', fontWeight: 600, color: 'var(--color-success)' }}>
                  Pronto para Inspeção
                </p>
              </div>
            </div>

            <div>
              <h3 style={{ fontSize: 'var(--font-size-sm)', fontWeight: 700, color: 'var(--color-text-secondary)', marginBottom: 'var(--space-2)' }}>
                Capacidades do Núcleo Habilitadas
              </h3>
              {renderCapabilities(coreInfo.capabilities)}
            </div>

            <p style={{ fontSize: 'var(--font-size-xs)', color: 'var(--color-text-muted)', fontStyle: 'italic', marginTop: 'var(--space-2)' }}>
              Nota de escopo: xblob foca em conformidade arquitetural, ferramentas de inspeção e preservação. Não há execução ou suporte a jogos comerciais nesta versão.
            </p>
          </div>
        ) : (
          <p style={{ color: 'var(--color-danger)' }}>Não foi possível obter dados da ABI C do núcleo.</p>
        )}
      </Card>
    </div>
  );
};
