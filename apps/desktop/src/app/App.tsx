import React, { useEffect, useState } from 'react';
import { CoreInfo, MediaReport } from '../bridge/types';
import { getBridge } from '../bridge';
import { SkipLink } from '../components/SkipLink';
import { ThemeToggle } from '../components/ThemeToggle';
import { HomeScreen } from '../features/home/HomeScreen';
import { InspectionView } from '../features/inspection/InspectionView';
import { AboutModal } from '../features/about/AboutModal';

type CurrentView = 'home' | 'inspection';

export const App: React.FC = () => {
  const [currentView, setCurrentView] = useState<CurrentView>('home');
  const [coreInfo, setCoreInfo] = useState<CoreInfo | null>(null);
  const [isLoadingCoreInfo, setIsLoadingCoreInfo] = useState<boolean>(true);

  // Inspection states
  const [report, setReport] = useState<MediaReport | null>(null);
  const [inspectionError, setInspectionError] = useState<{ code: string; message: string } | null>(null);
  const [isInspecting, setIsInspecting] = useState<boolean>(false);

  // About modal state
  const [isAboutOpen, setIsAboutOpen] = useState<boolean>(false);

  useEffect(() => {
    const bridge = getBridge();
    bridge
      .getCoreInfo()
      .then((info) => {
        setCoreInfo(info);
      })
      .catch((err) => {
        console.error('Failed to load core info from bridge:', err);
      })
      .finally(() => {
        setIsLoadingCoreInfo(false);
      });
  }, []);

  const handlePickFileAndInspect = async () => {
    const bridge = getBridge();
    try {
      const selectedPath = await bridge.pickFile();
      if (!selectedPath) {
        return; // User cancelled dialog
      }

      setCurrentView('inspection');
      setIsInspecting(true);
      setInspectionError(null);

      const inspectionReport = await bridge.inspectMedia(selectedPath);
      setReport(inspectionReport);
    } catch (err: unknown) {
      let code = 'INTERNAL_ERROR';
      let message = 'Falha desconhecida na inspeção';

      if (err instanceof Error) {
        message = err.message;
        if ('code' in err && typeof (err as { code: unknown }).code === 'string') {
          code = (err as { code: string }).code;
        }
      }

      setInspectionError({ code, message });
      setReport(null);
    } finally {
      setIsInspecting(false);
    }
  };

  const handleResetInspection = () => {
    setReport(null);
    setInspectionError(null);
    setIsInspecting(false);
    setCurrentView('home');
  };

  return (
    <>
      <SkipLink targetId="main-content" />

      {/* Header */}
      <header
        role="banner"
        style={{
          borderBottom: '1px solid var(--color-border-subtle)',
          backgroundColor: 'var(--color-bg-surface)',
          padding: 'var(--space-3) var(--space-6)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          boxShadow: 'var(--shadow-sm)',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: 'var(--space-3)' }}>
          <button
            type="button"
            onClick={() => setCurrentView('home')}
            style={{
              background: 'none',
              border: 'none',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: 'var(--space-2)',
              padding: 0,
            }}
          >
            <span
              style={{
                width: '32px',
                height: '32px',
                backgroundColor: 'var(--color-primary)',
                borderRadius: 'var(--radius-md)',
                display: 'inline-flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: 'var(--color-primary-contrast)',
                fontWeight: 900,
                fontSize: 'var(--font-size-lg)',
              }}
            >
              X
            </span>
            <span style={{ fontSize: 'var(--font-size-xl)', fontWeight: 800, color: 'var(--color-text-primary)' }}>
              xblob
            </span>
          </button>
        </div>

        {/* Navigation Bar */}
        <nav aria-label="Navegação principal">
          <ul
            style={{
              listStyle: 'none',
              display: 'flex',
              gap: 'var(--space-2)',
              alignItems: 'center',
            }}
          >
            <li>
              <button
                type="button"
                onClick={() => setCurrentView('home')}
                aria-current={currentView === 'home' ? 'page' : undefined}
                style={{
                  padding: 'var(--space-2) var(--space-3)',
                  border: 'none',
                  background: currentView === 'home' ? 'var(--color-bg-surface-hover)' : 'none',
                  borderRadius: 'var(--radius-md)',
                  fontWeight: currentView === 'home' ? 700 : 500,
                  color: currentView === 'home' ? 'var(--color-primary)' : 'var(--color-text-secondary)',
                  cursor: 'pointer',
                }}
              >
                Início
              </button>
            </li>
            <li>
              <button
                type="button"
                onClick={() => {
                  setCurrentView('inspection');
                  if (!report && !inspectionError && !isInspecting) {
                    handlePickFileAndInspect();
                  }
                }}
                aria-current={currentView === 'inspection' ? 'page' : undefined}
                style={{
                  padding: 'var(--space-2) var(--space-3)',
                  border: 'none',
                  background: currentView === 'inspection' ? 'var(--color-bg-surface-hover)' : 'none',
                  borderRadius: 'var(--radius-md)',
                  fontWeight: currentView === 'inspection' ? 700 : 500,
                  color: currentView === 'inspection' ? 'var(--color-primary)' : 'var(--color-text-secondary)',
                  cursor: 'pointer',
                }}
              >
                Inspeção
              </button>
            </li>
            <li>
              <button
                type="button"
                onClick={() => setIsAboutOpen(true)}
                style={{
                  padding: 'var(--space-2) var(--space-3)',
                  border: 'none',
                  background: 'none',
                  borderRadius: 'var(--radius-md)',
                  fontWeight: 500,
                  color: 'var(--color-text-secondary)',
                  cursor: 'pointer',
                }}
              >
                Sobre
              </button>
            </li>
            <li style={{ marginLeft: 'var(--space-2)' }}>
              <ThemeToggle />
            </li>
          </ul>
        </nav>
      </header>

      {/* Main Content Area */}
      <main
        id="main-content"
        role="main"
        tabIndex={-1}
        style={{
          flex: 1,
          padding: 'var(--space-6)',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
        }}
      >
        {currentView === 'home' ? (
          <HomeScreen
            coreInfo={coreInfo}
            isLoadingCoreInfo={isLoadingCoreInfo}
            onStartInspection={handlePickFileAndInspect}
            onOpenAbout={() => setIsAboutOpen(true)}
          />
        ) : (
          <div style={{ width: '100%', maxWidth: '900px' }}>
            <InspectionView
              report={report}
              error={inspectionError}
              isLoading={isInspecting}
              onPickFile={handlePickFileAndInspect}
              onReset={handleResetInspection}
            />
          </div>
        )}
      </main>

      {/* Footer */}
      <footer
        role="contentinfo"
        style={{
          borderTop: '1px solid var(--color-border-subtle)',
          backgroundColor: 'var(--color-bg-surface)',
          padding: 'var(--space-4) var(--space-6)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: 'var(--font-size-xs)',
          color: 'var(--color-text-muted)',
        }}
      >
        <div>
          <span>xblob &copy; {new Date().getFullYear()} — Projeto de Preservação e Pesquisa Arquitetural</span>
        </div>
        <div>
          <span>Sem fins lucrativos &bull; Código aberto</span>
        </div>
      </footer>

      {/* About Modal */}
      <AboutModal
        isOpen={isAboutOpen}
        onClose={() => setIsAboutOpen(false)}
        coreInfo={coreInfo}
      />
    </>
  );
};
