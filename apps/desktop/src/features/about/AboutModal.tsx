import React from 'react';
import { Modal } from '../../components/Modal';
import { CoreInfo } from '../../bridge/types';
import { Badge } from '../../components/Badge';

export interface AboutModalProps {
  isOpen: boolean;
  onClose: () => void;
  coreInfo: CoreInfo | null;
}

export const AboutModal: React.FC<AboutModalProps> = ({
  isOpen,
  onClose,
  coreInfo,
}) => {
  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Sobre o xblob"
      descriptionId="about-description"
    >
      <div id="about-description" style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-4)' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-2)' }}>
          <h3 style={{ fontSize: 'var(--font-size-lg)', fontWeight: 700, color: 'var(--color-text-primary)' }}>
            xblob Desktop
          </h3>
          <Badge variant="info">v0.1.0</Badge>
        </div>

        <p style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-secondary)', lineHeight: 'var(--line-height-normal)' }}>
          Interface gráfica para inspeção de arquivos e validação do ambiente de emulação modular Xbox clássico.
        </p>

        <div
          style={{
            padding: 'var(--space-3)',
            backgroundColor: 'var(--color-bg-surface)',
            borderRadius: 'var(--radius-md)',
            border: '1px solid var(--color-border-subtle)',
            fontSize: 'var(--font-size-sm)',
          }}
        >
          <p><strong>Núcleo C++:</strong> {coreInfo?.productName || 'xblob'} v{coreInfo?.productVersion || '0.1.0'}</p>
          <p><strong>ABI C Estável:</strong> v{coreInfo ? `${coreInfo.abiVersionMajor}.${coreInfo.abiVersionMinor}.${coreInfo.abiVersionPatch}` : '1.0.0'}</p>
          <p><strong>Licença:</strong> MIT License (Código Aberto)</p>
        </div>

        <section aria-labelledby="scope-disclaimer-heading">
          <h4
            id="scope-disclaimer-heading"
            style={{
              fontSize: 'var(--font-size-sm)',
              fontWeight: 700,
              color: 'var(--color-text-primary)',
              marginBottom: 'var(--space-1)',
            }}
          >
            Aviso Legal e Limites de Escopo
          </h4>
          <p
            style={{
              fontSize: 'var(--font-size-xs)',
              color: 'var(--color-text-muted)',
              lineHeight: 'var(--line-height-normal)',
              borderLeft: '3px solid var(--color-border-strong)',
              paddingLeft: 'var(--space-3)',
            }}
          >
            xblob é um projeto independente voltado à pesquisa arquitetural e preservação digital. Este projeto não é afiliado, associado, autorizado, endossado ou de qualquer forma oficialmente conectado à Microsoft Corporation. O software não inclui, não distribui e não requer software proprietário ou BIOS protegida por direitos autorais. Esta versão de fundação destina-se exclusivamente à análise e inspeção de cabeçalhos e não suporta a execução interativa de jogos comerciais.
          </p>
        </section>

        <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: 'var(--space-2)' }}>
          <button
            type="button"
            onClick={onClose}
            style={{
              padding: 'var(--space-2) var(--space-4)',
              backgroundColor: 'var(--color-primary)',
              color: 'var(--color-primary-contrast)',
              border: 'none',
              borderRadius: 'var(--radius-md)',
              fontWeight: 600,
              cursor: 'pointer',
            }}
          >
            Entendido
          </button>
        </div>
      </div>
    </Modal>
  );
};
