import React from 'react';

export interface AlertProps extends React.HTMLAttributes<HTMLDivElement> {
  variant?: 'info' | 'success' | 'warning' | 'error';
  title?: string;
  onClose?: () => void;
}

export const Alert: React.FC<AlertProps> = ({
  variant = 'info',
  title,
  children,
  onClose,
  style,
  ...props
}) => {
  const isError = variant === 'error';

  const getTheme = () => {
    switch (variant) {
      case 'error':
        return {
          bg: 'var(--color-danger-surface)',
          border: 'var(--color-danger)',
          text: 'var(--color-danger)',
          icon: '⚠️',
        };
      case 'warning':
        return {
          bg: 'var(--color-warning-surface)',
          border: 'var(--color-warning)',
          text: 'var(--color-warning)',
          icon: '⚡',
        };
      case 'success':
        return {
          bg: 'var(--color-success-surface)',
          border: 'var(--color-success)',
          text: 'var(--color-success)',
          icon: '✓',
        };
      case 'info':
      default:
        return {
          bg: 'var(--color-bg-surface)',
          border: 'var(--color-border-strong)',
          text: 'var(--color-text-primary)',
          icon: 'ℹ',
        };
    }
  };

  const theme = getTheme();

  return (
    <div
      role={isError ? 'alert' : 'status'}
      aria-live={isError ? 'assertive' : 'polite'}
      style={{
        backgroundColor: theme.bg,
        border: `1px solid ${theme.border}`,
        borderRadius: 'var(--radius-md)',
        padding: 'var(--space-4)',
        display: 'flex',
        gap: 'var(--space-3)',
        alignItems: 'flex-start',
        color: 'var(--color-text-primary)',
        ...style,
      }}
      {...props}
    >
      <span aria-hidden="true" style={{ fontSize: '1.2em', lineHeight: 1 }}>
        {theme.icon}
      </span>
      <div style={{ flex: 1 }}>
        {title && (
          <h3
            style={{
              fontSize: 'var(--font-size-base)',
              fontWeight: 700,
              color: theme.text,
              marginBottom: 'var(--space-1)',
            }}
          >
            {title}
          </h3>
        )}
        <div style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-secondary)' }}>
          {children}
        </div>
      </div>
      {onClose && (
        <button
          type="button"
          onClick={onClose}
          aria-label="Fechar notificação"
          style={{
            background: 'none',
            border: 'none',
            color: 'var(--color-text-muted)',
            cursor: 'pointer',
            fontSize: 'var(--font-size-lg)',
            lineHeight: 1,
            padding: '2px 4px',
          }}
        >
          ×
        </button>
      )}
    </div>
  );
};
