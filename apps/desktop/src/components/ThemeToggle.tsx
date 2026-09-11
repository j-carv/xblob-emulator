import React from 'react';
import { useTheme } from '../app/ThemeProvider';

export const ThemeToggle: React.FC = () => {
  const { theme, resolvedTheme, setTheme } = useTheme();

  const cycleTheme = () => {
    if (theme === 'system') {
      setTheme('light');
    } else if (theme === 'light') {
      setTheme('dark');
    } else {
      setTheme('system');
    }
  };

  const getIcon = () => {
    if (theme === 'system') return '💻 Sistema';
    if (resolvedTheme === 'dark') return '🌙 Escuro';
    return '☀️ Claro';
  };

  return (
    <button
      type="button"
      onClick={cycleTheme}
      aria-label={`Alternar tema. Tema atual: ${theme}. Tema aplicado: ${resolvedTheme}.`}
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 'var(--space-2)',
        padding: 'var(--space-1) var(--space-3)',
        borderRadius: 'var(--radius-md)',
        border: '1px solid var(--color-border-subtle)',
        backgroundColor: 'var(--color-bg-surface)',
        color: 'var(--color-text-primary)',
        fontSize: 'var(--font-size-sm)',
        fontWeight: 600,
        cursor: 'pointer',
        transition: 'all var(--transition-fast)',
      }}
    >
      <span aria-hidden="true">{getIcon()}</span>
    </button>
  );
};
