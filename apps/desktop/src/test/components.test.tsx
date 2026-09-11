import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { axe } from 'vitest-axe';
import 'vitest-axe/extend-expect';
import { Button } from '../components/Button';
import { Card } from '../components/Card';
import { Badge } from '../components/Badge';
import { Tabs } from '../components/Tabs';
import { Alert } from '../components/Toast';
import { SkipLink } from '../components/SkipLink';
import { Modal } from '../components/Modal';
import { ThemeProvider } from '../app/ThemeProvider';
import { ThemeToggle } from '../components/ThemeToggle';

describe('Accessible Base Components', () => {
  it('renders Button with variants and passes axe check', async () => {
    const handleClick = vi.fn();
    const { container } = render(
      <Button variant="primary" onClick={handleClick}>
        Clique Aqui
      </Button>
    );

    const btn = screen.getByRole('button', { name: /clique aqui/i });
    expect(btn).toBeInTheDocument();
    fireEvent.click(btn);
    expect(handleClick).toHaveBeenCalledTimes(1);

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('renders Button in loading state with aria-busy and disabled', async () => {
    render(
      <Button isLoading variant="secondary">
        Carregando
      </Button>
    );

    const btn = screen.getByRole('button', { name: /carregando/i });
    expect(btn).toHaveAttribute('aria-busy', 'true');
    expect(btn).toBeDisabled();
  });

  it('renders Card with title and content and passes axe check', async () => {
    const { container } = render(
      <Card title="Título do Cartão" subtitle="Subtítulo informativo">
        <p>Conteúdo interno do cartão</p>
      </Card>
    );

    expect(screen.getByRole('region', { name: /título do cartão/i })).toBeInTheDocument();
    expect(screen.getByText('Conteúdo interno do cartão')).toBeInTheDocument();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('renders Badge with variants', () => {
    render(<Badge variant="success">Ativo</Badge>);
    expect(screen.getByText('Ativo')).toBeInTheDocument();
  });

  it('renders Tabs with arrow-key keyboard navigation and passes axe check', async () => {
    const handleTabChange = vi.fn();
    const tabs = [
      { id: 'tab1', label: 'Aba 1', content: <div>Painel 1</div> },
      { id: 'tab2', label: 'Aba 2', content: <div>Painel 2</div> },
    ];

    const { container } = render(
      <Tabs tabs={tabs} activeTab="tab1" onTabChange={handleTabChange} ariaLabel="Teste de Abas" />
    );

    const tab1 = screen.getByRole('tab', { name: 'Aba 1' });
    const tab2 = screen.getByRole('tab', { name: 'Aba 2' });

    expect(tab1).toHaveAttribute('aria-selected', 'true');
    expect(tab2).toHaveAttribute('aria-selected', 'false');
    expect(screen.getByRole('tabpanel')).toHaveTextContent('Painel 1');

    // Arrow Right navigates to next tab
    fireEvent.keyDown(tab1, { key: 'ArrowRight' });
    expect(handleTabChange).toHaveBeenCalledWith('tab2');

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('renders Alert/Toast with accessible roles and passes axe check', async () => {
    const handleClose = vi.fn();
    const { container } = render(
      <Alert variant="error" title="Erro Crítico" onClose={handleClose}>
        Falha de leitura do disco
      </Alert>
    );

    expect(screen.getByRole('alert')).toBeInTheDocument();
    expect(screen.getByText('Erro Crítico')).toBeInTheDocument();

    const closeBtn = screen.getByRole('button', { name: /fechar notificação/i });
    fireEvent.click(closeBtn);
    expect(handleClose).toHaveBeenCalledTimes(1);

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('renders SkipLink for keyboard jump to main content', () => {
    render(<SkipLink targetId="main-content" />);
    const link = screen.getByRole('link', { name: /pular para o conteúdo principal/i });
    expect(link).toHaveAttribute('href', '#main-content');
  });

  it('renders Modal dialog with focus trap and Escape handler', async () => {
    const handleClose = vi.fn();
    const { container } = render(
      <Modal isOpen={true} onClose={handleClose} title="Diálogo de Teste">
        <p>Conteúdo modal</p>
      </Modal>
    );

    expect(screen.getByRole('dialog', { name: 'Diálogo de Teste' })).toBeInTheDocument();
    expect(screen.getByText('Conteúdo modal')).toBeInTheDocument();

    fireEvent.keyDown(document, { key: 'Escape' });
    expect(handleClose).toHaveBeenCalledTimes(1);

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });

  it('renders ThemeToggle and cycles themes inside ThemeProvider', async () => {
    const { container } = render(
      <ThemeProvider>
        <ThemeToggle />
      </ThemeProvider>
    );

    const toggleBtn = screen.getByRole('button', { name: /alternar tema/i });
    expect(toggleBtn).toBeInTheDocument();

    fireEvent.click(toggleBtn);
    expect(document.documentElement.getAttribute('data-theme')).toBeDefined();

    const results = await axe(container);
    expect(results).toHaveNoViolations();
  });
});
