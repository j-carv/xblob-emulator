## Why

O núcleo já prepara XBE direto e possui CPU, kernel HLE, PCI e GPU diagnóstica, mas imagens ISO/XISO ainda são apenas inspecionadas. Para avançar rumo a jogos fornecidos pelo usuário, o emulador precisa montar XDVDFS, localizar `default.xbe`, servir arquivos ao guest e unificar o pipeline de preparação de mídia.

## What Changes

- Implementar parser/navegador XDVDFS defensivo com diretórios, arquivos e leitura por ranges.
- Criar `libs/vfs` com volumes, caminhos Xbox, handles geracionais e mounts read-only.
- Implementar pipeline de mídia que aceita `.xbe`, `.iso` e `.xiso`, localiza `default.xbe` e prepara `MachineSession` sem cópias integrais desnecessárias.
- Expandir Kernel HLE com operações iniciais de arquivo/diretório, seek, query e close sobre VFS.
- Expor árvore XDVDFS, executable selecionado e progresso/erros pela ABI C 1.4 e desktop.
- Manter conteúdo do usuário local, sem telemetria/upload, e usar somente fixtures sintéticas nos testes.

## Capabilities

### New Capabilities
- `xdvdfs-filesystem`: Montagem e navegação defensiva de volumes XDVDFS.
- `virtual-filesystem`: VFS portátil com mounts, normalização de caminhos e handles seguros.
- `media-boot-pipeline`: Preparação unificada de XBE direto e XBE contido em ISO/XISO.
- `kernel-file-services`: Serviços HLE iniciais de arquivo sobre VFS.

### Modified Capabilities
- `machine-session`: Possuir contexto de mídia/VFS e preparar sessão por fonte de boot.
- `desktop-media-inspection`: Navegar conteúdo e iniciar preparação de mídia suportada.

## Impact

Cria `libs/vfs`, amplia formats/io/loader/kernel/machine/C ABI/Tauri/React e CI. Este marco prepara mídia real do usuário, mas não garante ainda que títulos iniciem ou sejam jogáveis; compatibilidade será declarada apenas por testes posteriores.