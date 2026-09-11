import React from 'react';

export interface BadgeProps extends React.HTMLAttributes<HTMLSpanElement> {
  variant?: 'default' | 'success' | 'warning' | 'danger' | 'info';
}

export const Badge: React.FC<BadgeProps> = ({
  children,
  variant = 'default',
  style,
  ...props
}) => {
  const getColors = () => {
    switch (variant) {
      case 'success':
        return {
          bg: 'var(--color-success-surface)',
          text: 'var(--color-success)',
          border: 'var(--color-success)',
        };
      case 'warning':
        return {
          bg: 'var(--color-warning-surface)',
          text: 'var(--color-warning)',
          border: 'var(--color-warning)',
        };
      case 'danger':
        return {
          bg: 'var(--color-danger-surface)',
          text: 'var(--color-danger)',
          border: 'var(--color-danger)',
        };
      case 'info':
        return {
          bg: 'var(--color-bg-muted)',
          text: 'var(--color-accent)',
          border: 'var(--color-accent)',
        };
      case 'default':
      default:
        return {
          bg: 'var(--color-bg-muted)',
          text: 'var(--color-text-secondary)',
          border: 'var(--color-border-subtle)',
        };
    }
  };

  const colors = getColors();

  return (
    <span
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        padding: '2px 8px',
        borderRadius: 'var(--radius-full)',
        fontSize: 'var(--font-size-xs)',
        fontWeight: 600,
        backgroundColor: colors.bg,
        color: colors.text,
        border: `1px solid ${colors.border}`,
        textTransform: 'uppercase',
        letterSpacing: '0.05em',
        ...style,
      }}
      {...props}
    >
      {children}
    </span>
  );
};
