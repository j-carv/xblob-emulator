import React, { useRef } from 'react';

export interface TabItem {
  id: string;
  label: string;
  content: React.ReactNode;
}

export interface TabsProps {
  tabs: TabItem[];
  activeTab: string;
  onTabChange: (id: string) => void;
  ariaLabel?: string;
}

export const Tabs: React.FC<TabsProps> = ({
  tabs,
  activeTab,
  onTabChange,
  ariaLabel = 'Abas de conteúdo',
}) => {
  const tabRefs = useRef<Record<string, HTMLButtonElement | null>>({});

  const handleKeyDown = (e: React.KeyboardEvent, index: number) => {
    let targetIndex = -1;
    if (e.key === 'ArrowRight') {
      targetIndex = (index + 1) % tabs.length;
    } else if (e.key === 'ArrowLeft') {
      targetIndex = (index - 1 + tabs.length) % tabs.length;
    } else if (e.key === 'Home') {
      targetIndex = 0;
    } else if (e.key === 'End') {
      targetIndex = tabs.length - 1;
    }

    if (targetIndex >= 0) {
      e.preventDefault();
      const targetId = tabs[targetIndex].id;
      onTabChange(targetId);
      tabRefs.current[targetId]?.focus();
    }
  };

  const currentTab = tabs.find((t) => t.id === activeTab) || tabs[0];

  return (
    <div>
      <div
        role="tablist"
        aria-label={ariaLabel}
        style={{
          display: 'flex',
          gap: 'var(--space-2)',
          borderBottom: '2px solid var(--color-border-subtle)',
          marginBottom: 'var(--space-4)',
        }}
      >
        {tabs.map((tab, idx) => {
          const isSelected = tab.id === (currentTab?.id);
          return (
            <button
              key={tab.id}
              ref={(el) => {
                tabRefs.current[tab.id] = el;
              }}
              role="tab"
              id={`tab-${tab.id}`}
              aria-selected={isSelected}
              aria-controls={`tabpanel-${tab.id}`}
              tabIndex={isSelected ? 0 : -1}
              onClick={() => onTabChange(tab.id)}
              onKeyDown={(e) => handleKeyDown(e, idx)}
              style={{
                padding: 'var(--space-2) var(--space-4)',
                border: 'none',
                background: 'none',
                color: isSelected ? 'var(--color-primary)' : 'var(--color-text-secondary)',
                fontWeight: isSelected ? 700 : 500,
                borderBottom: isSelected ? '3px solid var(--color-primary)' : '3px solid transparent',
                marginBottom: '-2px',
                cursor: 'pointer',
                transition: 'all var(--transition-fast)',
              }}
            >
              {tab.label}
            </button>
          );
        })}
      </div>

      {currentTab && (
        <div
          role="tabpanel"
          id={`tabpanel-${currentTab.id}`}
          aria-labelledby={`tab-${currentTab.id}`}
          tabIndex={0}
        >
          {currentTab.content}
        </div>
      )}
    </div>
  );
};
