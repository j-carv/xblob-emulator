import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import { axe } from 'vitest-axe';
import 'vitest-axe/extend-expect';
import { HomeScreen } from '../features/home/HomeScreen';
import { InspectionView } from '../features/inspection/InspectionView';
import { AboutModal } from '../features/about/AboutModal';
import { App } from '../app/App';
import { ThemeProvider } from '../app/ThemeProvider';
import { MediaReport, CoreInfo } from '../bridge/types';

describe('Feature Views and Accessibility', () => {
  const mockCoreInfo: CoreInfo = {
    abiVersionMajor: 1,
    abiVersionMinor: 0,
    abiVersionPatch: 0,
    capabilities: 1 | 2 | 4 | 8,
    productName: 'xblob',
    productVersion: '0.1.0',
  };

  const mockXbeReport: MediaReport = {
    type: 'xbe',
    filePath: '/games/halo/default.xbe',
    fileSize: 3145728,
    humanSummary: 'xblob Relatório de Inspeção de Mídia\nArquivo: /games/halo/default.xbe',
    xbe: {
      titleName: 'Halo: Combat Evolved',
      titleId: 0x4d530004,
      titleIdHex: '0x4D530004',
      diskNumber: 1,
      gameRegion: 1,
      entryPoint: 0x00011000,
      entryPointHex: '0x00011000',
      sectionCount: 3,
    },
  };

  it('HomeScreen renders core info, capabilities, and passes axe check without play/run actions', async () => {
    const startInspect = vi.fn();
    const openAbout = vi.fn();

    const { container } = render(
      <HomeScreen
        coreInfo={mockCoreInfo}
        isLoadingCoreInfo={false}
        onStartInspection={startInspect}
        onOpenAbout={openAbout}
      />
    );

    expect(screen.getByRole('heading', { name: 'xblob Desktop' })).toBeInTheDocument();
    expect(screen.getByText(/xblob v0.1.0/i)).toBeInTheDocument();
    expect(screen.getByText(/v1.0.0/i)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /inspecionar mídia/i })).toBeInTheDocument();

    // Verify absence of any playable emulation or "Play/Run" actions
    expect(screen.queryByRole('button', { name: /^jogar$/i })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^play$/i })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^executar jogo$/i })).not.toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders initial empty state and triggers file picker', async () => {
    const handlePick = vi.fn();
    const handleReset = vi.fn();

    const { container } = render(
      <InspectionView
        report={null}
        error={null}
        isLoading={false}
        onPickFile={handlePick}
        onReset={handleReset}
      />
    );

    const pickBtn = screen.getByRole('button', { name: /selecionar arquivo do disco/i });
    expect(pickBtn).toBeInTheDocument();
    fireEvent.click(pickBtn);
    expect(handlePick).toHaveBeenCalledTimes(1);

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders loading state with accessible aria-busy / status', async () => {
    const { container } = render(
      <InspectionView
        report={null}
        error={null}
        isLoading={true}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    expect(screen.getByRole('status')).toBeInTheDocument();
    expect(screen.getByText(/lendo e analisando arquivo através da abi c/i)).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders error state with friendly message and retry button', async () => {
    const handlePick = vi.fn();
    const { container } = render(
      <InspectionView
        report={null}
        error={{ code: 'UNSUPPORTED_FORMAT', message: 'Formato não reconhecido' }}
        isLoading={false}
        onPickFile={handlePick}
        onReset={vi.fn()}
      />
    );

    expect(screen.getByRole('alert')).toBeInTheDocument();
    expect(screen.getByText('Formato de Mídia Não Reconhecido')).toBeInTheDocument();

    const retryBtn = screen.getByRole('button', { name: /tentar outro arquivo/i });
    fireEvent.click(retryBtn);
    expect(handlePick).toHaveBeenCalledTimes(1);

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders XBE inspection results with tabs and passes axe check', async () => {
    const { container } = render(
      <InspectionView
        report={mockXbeReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    expect(screen.getByRole('tab', { name: 'Resumo Geral' })).toBeInTheDocument();
    expect(screen.getByRole('tab', { name: 'Executável XBE' })).toBeInTheDocument();
    expect(screen.getByRole('tab', { name: 'Relatório Completo' })).toBeInTheDocument();

    // Click on XBE tab
    fireEvent.click(screen.getByRole('tab', { name: 'Executável XBE' }));
    expect(screen.getAllByText('Halo: Combat Evolved').length).toBeGreaterThanOrEqual(1);
    expect(screen.getByText('0x4D530004')).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders validation & preparation panel, handles preparation flow and passes axe check without play/run actions', async () => {
    const { container } = render(
      <InspectionView
        report={mockXbeReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    const validationTab = screen.getByRole('tab', { name: 'Validação & Preparação' });
    expect(validationTab).toBeInTheDocument();
    fireEvent.click(validationTab);

    expect(
      screen.getByRole('heading', { name: 'Sessão Diagnóstica de Memória e Loader' })
    ).toBeInTheDocument();
    const prepareBtn = screen.getByRole('button', { name: 'Validar e Preparar Sessão' });
    expect(prepareBtn).toBeInTheDocument();

    // Verify absence of Play/Run actions
    expect(screen.queryByRole('button', { name: /^jogar$/i })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^play$/i })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^run$/i })).not.toBeInTheDocument();

    fireEvent.click(prepareBtn);

    await waitFor(() => {
      expect(screen.getByText(/sessão pronta/i)).toBeInTheDocument();
    });

    expect(screen.getByText('0x00011000')).toBeInTheDocument();
    expect(screen.getByText(/64 MiB Retail/i)).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView displays ineligible explanation on Framebuffer NV2A tab for commercial media and passes axe check', async () => {
    const { container } = render(
      <InspectionView
        report={mockXbeReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    const displayTab = screen.getByRole('tab', { name: 'Framebuffer NV2A' });
    expect(displayTab).toBeInTheDocument();
    fireEvent.click(displayTab);

    expect(screen.getByText('Mídia Não Suportada Neste Marco')).toBeInTheDocument();
    expect(screen.getByText(/Visualização Gráfica Indisponível para Mídia Geral Neste Marco/i)).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^play$/i })).not.toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView displays active framebuffer preview for eligible synthetic media and passes axe check', async () => {
    const mockSyntheticReport: MediaReport = {
      type: 'xbe',
      filePath: '/synthetic/test_fixture.xbe',
      fileSize: 4096,
      humanSummary: 'Synthetic fixture',
      xbe: {
        titleName: 'Synthetic Display Test',
        sectionCount: 1,
      },
    };

    const { container } = render(
      <InspectionView
        report={mockSyntheticReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    // 1. Prepare session
    fireEvent.click(screen.getByRole('tab', { name: 'Validação & Preparação' }));
    fireEvent.click(screen.getByRole('button', { name: 'Validar e Preparar Sessão' }));
    await waitFor(() => {
      expect(screen.getByText(/sessão pronta/i)).toBeInTheDocument();
    });

    // 2. Open Framebuffer NV2A tab
    fireEvent.click(screen.getByRole('tab', { name: 'Framebuffer NV2A' }));

    await waitFor(() => {
      expect(screen.getByRole('img', { name: /framebuffer diagnóstico nv2a/i })).toBeInTheDocument();
    });

    expect(screen.getByText(/Frame Ativo/i)).toBeInTheDocument();
    expect(screen.getByText(/RGBA8 Linear/i)).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('AboutModal displays legal scope disclaimer and passes axe check', async () => {
    const handleClose = vi.fn();
    const { container } = render(
      <AboutModal
        isOpen={true}
        onClose={handleClose}
        coreInfo={mockCoreInfo}
      />
    );

    expect(screen.getByRole('heading', { name: 'Sobre o xblob' })).toBeInTheDocument();
    expect(screen.getByText(/aviso legal e limites de escopo/i)).toBeInTheDocument();
    expect(screen.getByText(/projeto independente voltado à pesquisa arquitetural/i)).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('App full flow renders and navigates without accessibility violations', async () => {
    const { container } = render(
      <ThemeProvider>
        <App />
      </ThemeProvider>
    );

    // Initial load: Home screen
    expect(screen.getByRole('banner')).toBeInTheDocument();
    expect(screen.getByRole('main')).toBeInTheDocument();
    expect(screen.getByRole('contentinfo')).toBeInTheDocument();

    // Wait for mock bridge getCoreInfo to resolve
    await waitFor(() => {
      expect(screen.getByText(/xblob v0.1.0/i)).toBeInTheDocument();
    });

    // Navigate to Inspection
    const navInspectBtn = screen.getByRole('button', { name: /^inspeção$/i });
    fireEvent.click(navInspectBtn);

    await waitFor(() => {
      expect(screen.getByRole('heading', { name: 'Inspecionar Mídia Xbox' })).toBeInTheDocument();
    });

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders "Preparar Mídia" flow with distinct non-play action and passes axe check', async () => {
    const { container } = render(
      <InspectionView
        report={mockXbeReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    const mediaBootTab = screen.getByRole('tab', { name: 'Preparar Mídia' });
    expect(mediaBootTab).toBeInTheDocument();
    fireEvent.click(mediaBootTab);

    expect(
      screen.getByRole('heading', { name: 'Preparação de Mídia e Montagem VFS' })
    ).toBeInTheDocument();

    const prepareBtn = screen.getByRole('button', { name: 'Preparar mídia' });
    expect(prepareBtn).toBeInTheDocument();

    // Verify absence of play/run actions
    expect(screen.queryByRole('button', { name: /^jogar$/i })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^play$/i })).not.toBeInTheDocument();

    fireEvent.click(prepareBtn);

    await waitFor(() => {
      expect(screen.getByText('Mídia Pronta (Prepared)')).toBeInTheDocument();
    });

    expect(screen.getByText('VFS: D:\\ Montado (Read-Only)')).toBeInTheDocument();
    expect(screen.getByText('DEFAULT.XBE')).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('InspectionView renders XDVDFS Browser for ISO media with pagination, filtering and passes axe check', async () => {
    const mockIsoReport: MediaReport = {
      type: 'xiso_trimmed',
      filePath: '/games/clean_room/game.iso',
      fileSize: 4700372992,
      humanSummary: 'XDVDFS ISO report',
      xiso: {
        variant: 'XisoTrimmed',
        volumeDescriptorOffset: 0x10000,
        rootDirSector: 0x200,
        rootDirSize: 4096,
        validFooterMagic: true,
      },
    };

    const { container } = render(
      <InspectionView
        report={mockIsoReport}
        error={null}
        isLoading={false}
        onPickFile={vi.fn()}
        onReset={vi.fn()}
      />
    );

    const browserTab = screen.getByRole('tab', { name: 'Navegador XDVDFS' });
    expect(browserTab).toBeInTheDocument();
    fireEvent.click(browserTab);

    await waitFor(() => {
      expect(screen.getByRole('heading', { name: 'Navegador de Arquivos XDVDFS' })).toBeInTheDocument();
    });

    await waitFor(() => {
      expect(screen.getByText('DEFAULT.XBE')).toBeInTheDocument();
    });

    // Verify filter input
    const filterInput = screen.getByRole('searchbox', { name: 'Filtrar entradas do diretório' });
    expect(filterInput).toBeInTheDocument();

    fireEvent.change(filterInput, { target: { value: 'DEFAULT' } });
    expect(screen.getByText('DEFAULT.XBE')).toBeInTheDocument();

    // Verify pagination controls
    expect(screen.getByRole('button', { name: 'Página anterior' })).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Próxima página' })).toBeEnabled();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });
});
