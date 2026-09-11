import React, { useEffect, useState, useCallback, useMemo } from 'react';
import { getBridge } from '../../bridge';
import { VfsDirectoryPage, VfsEntry } from '../../bridge/types';
import { Alert } from '../../components/Toast';
import { Button } from '../../components/Button';
import { Badge } from '../../components/Badge';

export interface XdvdfsBrowserProps {
  filePath: string;
}

export const XdvdfsBrowser: React.FC<XdvdfsBrowserProps> = ({ filePath }) => {
  const [currentDir, setCurrentDir] = useState<string>('');
  const [page, setPage] = useState<number>(0);
  const [pageSize] = useState<number>(10);
  const [pageData, setPageData] = useState<VfsDirectoryPage | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(false);
  const [error, setError] = useState<{ code: string; message: string } | null>(null);
  const [filterText, setFilterText] = useState<string>('');

  const fetchDirectoryPage = useCallback(
    async (dir: string, pageNum: number) => {
      setIsLoading(true);
      setError(null);
      try {
        const bridge = getBridge();
        const offset = pageNum * pageSize;
        const res = await bridge.browseMediaVfs(filePath, dir, offset, pageSize);
        setPageData(res);
      } catch (err: unknown) {
        const message =
          err instanceof Error ? err.message : 'Falha ao ler diretório XDVDFS';
        const code =
          err && typeof err === 'object' && 'code' in err
            ? String((err as { code: unknown }).code)
            : 'INTERNAL_ERROR';
        setError({ code, message });
        setPageData(null);
      } finally {
        setIsLoading(false);
      }
    },
    [filePath, pageSize]
  );

  useEffect(() => {
    setPage(0);
    void fetchDirectoryPage(currentDir, 0);
  }, [currentDir, fetchDirectoryPage]);

  const handlePageChange = (newPage: number) => {
    setPage(newPage);
    void fetchDirectoryPage(currentDir, newPage);
  };

  const handleDirectoryClick = (subDirName: string) => {
    const nextDir = currentDir ? `${currentDir}/${subDirName}` : subDirName;
    setCurrentDir(nextDir);
  };

  const handleNavigateUp = () => {
    if (!currentDir) return;
    const parts = currentDir.split('/');
    parts.pop();
    setCurrentDir(parts.join('/'));
  };

  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '-';
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GiB`;
  };

  const filteredEntries = useMemo(() => {
    if (!pageData) return [];
    if (!filterText.trim()) return pageData.entries;
    const term = filterText.toLowerCase();
    return pageData.entries.filter((entry: VfsEntry) =>
      entry.name.toLowerCase().includes(term)
    );
  }, [pageData, filterText]);

  const totalEntries = pageData?.totalCount ?? 0;
  const totalPages = Math.max(1, Math.ceil(totalEntries / pageSize));

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        gap: 'var(--space-4)',
        padding: 'var(--space-4)',
        backgroundColor: 'var(--color-bg-surface)',
        borderRadius: 'var(--radius-md)',
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: 'var(--space-2)',
        }}
      >
        <div>
          <h3 style={{ fontSize: 'var(--font-size-base)', fontWeight: 700 }}>
            Navegador de Arquivos XDVDFS
          </h3>
          <p
            style={{
              fontSize: 'var(--font-size-sm)',
              color: 'var(--color-text-muted)',
            }}
          >
            Diretório atual: <code>{currentDir ? `\\${currentDir}` : '\\ (raiz)'}</code>
          </p>
        </div>

        <div style={{ display: 'flex', gap: 'var(--space-2)', alignItems: 'center' }}>
          {currentDir && (
            <Button
              variant="secondary"
              onClick={handleNavigateUp}
              aria-label="Subir um diretório"
            >
              ↑ Subir nível
            </Button>
          )}
          <Button
            variant="secondary"
            onClick={() => void fetchDirectoryPage(currentDir, page)}
            disabled={isLoading}
            aria-label="Atualizar listagem"
          >
            {isLoading ? 'Atualizando...' : 'Recarregar'}
          </Button>
        </div>
      </div>

      <div style={{ display: 'flex', gap: 'var(--space-2)' }}>
        <input
          type="search"
          placeholder="Filtrar arquivos nesta página..."
          value={filterText}
          onChange={(e) => setFilterText(e.target.value)}
          aria-label="Filtrar entradas do diretório"
          style={{
            flex: 1,
            padding: 'var(--space-2) var(--space-3)',
            borderRadius: 'var(--radius-md)',
            border: '1px solid var(--color-border-subtle)',
            backgroundColor: 'var(--color-bg-canvas)',
            color: 'var(--color-text-primary)',
            fontSize: 'var(--font-size-sm)',
          }}
        />
      </div>

      {error && (
        <Alert variant="error" title="Erro ao Navegar no XDVDFS">
          <p>{error.message}</p>
          <p
            style={{
              fontSize: 'var(--font-size-xs)',
              color: 'var(--color-text-muted)',
              marginTop: 'var(--space-1)',
            }}
          >
            Código: <code>{error.code}</code>
          </p>
        </Alert>
      )}

      {isLoading && (
        <div
          role="status"
          aria-live="polite"
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            padding: 'var(--space-6)',
            gap: 'var(--space-3)',
          }}
        >
          <div
            aria-hidden="true"
            style={{
              width: '24px',
              height: '24px',
              border: '3px solid var(--color-border-subtle)',
              borderTopColor: 'var(--color-primary)',
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite',
            }}
          />
          <span style={{ fontSize: 'var(--font-size-sm)', color: 'var(--color-text-muted)' }}>
            Carregando diretório XDVDFS...
          </span>
        </div>
      )}

      {!isLoading && !error && pageData && (
        <>
          <div style={{ overflowX: 'auto' }}>
            <table
              aria-label="Tabela de arquivos e diretórios XDVDFS"
              style={{
                width: '100%',
                borderCollapse: 'collapse',
                fontSize: 'var(--font-size-sm)',
                textAlign: 'left',
              }}
            >
              <thead>
                <tr
                  style={{
                    borderBottom: '1px solid var(--color-border-subtle)',
                    color: 'var(--color-text-muted)',
                  }}
                >
                  <th style={{ padding: 'var(--space-2)' }}>Nome</th>
                  <th style={{ padding: 'var(--space-2)' }}>Tipo</th>
                  <th style={{ padding: 'var(--space-2)' }}>Tamanho</th>
                  <th style={{ padding: 'var(--space-2)' }}>Atributos</th>
                </tr>
              </thead>
              <tbody>
                {filteredEntries.length === 0 ? (
                  <tr>
                    <td
                      colSpan={4}
                      style={{
                        padding: 'var(--space-4)',
                        textAlign: 'center',
                        color: 'var(--color-text-muted)',
                      }}
                    >
                      Nenhum arquivo encontrado.
                    </td>
                  </tr>
                ) : (
                  filteredEntries.map((entry) => (
                    <tr
                      key={entry.name}
                      style={{
                        borderBottom: '1px solid var(--color-border-subtle)',
                      }}
                    >
                      <td style={{ padding: 'var(--space-2)' }}>
                        {entry.isDirectory ? (
                          <button
                            type="button"
                            onClick={() => handleDirectoryClick(entry.name)}
                            style={{
                              background: 'none',
                              border: 'none',
                              color: 'var(--color-primary)',
                              cursor: 'pointer',
                              fontWeight: 600,
                              padding: 0,
                              font: 'inherit',
                              textDecoration: 'underline',
                              textAlign: 'left',
                            }}
                            aria-label={`Abrir pasta ${entry.name}`}
                          >
                            <span aria-hidden="true">📁 </span>
                            <span>{entry.name}</span>
                          </button>
                        ) : (
                          <span>
                            <span aria-hidden="true">📄 </span>
                            <span>{entry.name}</span>
                          </span>
                        )}
                      </td>
                      <td style={{ padding: 'var(--space-2)' }}>
                        <Badge variant={entry.isDirectory ? 'info' : 'default'}>
                          {entry.isDirectory ? 'Diretório' : 'Arquivo'}
                        </Badge>
                      </td>
                      <td style={{ padding: 'var(--space-2)', fontFamily: 'var(--font-mono)' }}>
                        {entry.isDirectory ? '-' : formatBytes(entry.size)}
                      </td>
                      <td style={{ padding: 'var(--space-2)', fontFamily: 'var(--font-mono)' }}>
                        0x{entry.attributes.toString(16).toUpperCase().padStart(2, '0')}
                      </td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>

          <div
            style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              paddingTop: 'var(--space-3)',
              borderTop: '1px solid var(--color-border-subtle)',
              flexWrap: 'wrap',
              gap: 'var(--space-2)',
            }}
          >
            <span
              style={{
                fontSize: 'var(--font-size-xs)',
                color: 'var(--color-text-muted)',
              }}
            >
              Exibindo página {page + 1} de {totalPages} ({totalEntries} itens no total)
            </span>

            <div style={{ display: 'flex', gap: 'var(--space-2)' }}>
              <Button
                variant="secondary"
                onClick={() => handlePageChange(page - 1)}
                disabled={page === 0 || isLoading}
                aria-label="Página anterior"
              >
                Anterior
              </Button>
              <Button
                variant="secondary"
                onClick={() => handlePageChange(page + 1)}
                disabled={page + 1 >= totalPages || isLoading}
                aria-label="Próxima página"
              >
                Próxima
              </Button>
            </div>
          </div>
        </>
      )}
    </div>
  );
};
