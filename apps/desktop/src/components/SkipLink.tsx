import React from 'react';

export interface SkipLinkProps {
  targetId?: string;
  label?: string;
}

export const SkipLink: React.FC<SkipLinkProps> = ({
  targetId = 'main-content',
  label = 'Pular para o conteúdo principal',
}) => {
  return (
    <a href={`#${targetId}`} className="skip-link">
      {label}
    </a>
  );
};
