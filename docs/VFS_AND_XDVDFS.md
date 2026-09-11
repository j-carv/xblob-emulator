# Sistema de Arquivos Virtual (VFS) e XDVDFS (docs/VFS_AND_XDVDFS.md)

Este documento descreve a arquitetura, as garantias de segurança e o funcionamento dos subsistemas de I/O de disco do **xblob**: o sistema de arquivos **XDVDFS** (`libs/formats`), a camada de abstração de arquivos virtuais **VFS** (`libs/vfs`), o pipeline de boot de mídia (`libs/machine`), e os serviços de arquivos HLE do Kernel (`libs/kernel`).

---

## 1. Visão Geral

A meta do produto xblob é executar/jogar arquivos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário. No Marco 6, o emulador monta imagens de disco XDVDFS reais, realiza o streaming subrange de dados sem ler a ISO inteira na memória, navega case-insensitivamente pela árvore de diretórios, localiza o executável `default.xbe` e prepara a sessão de máquina para execução.

---

## 2. Subsistema XDVDFS (`libs/formats`)

### 2.1. Estrutura de Imagem e Detecção
O XDVDFS (Xbox Digital Video Disc File System) utiliza setores fixos de 2048 bytes:
- **ISO Raw (Redump/Dump integral)**: O descritor de volume encontra-se no setor 32 da partição de jogo (offset byte `32 * 2048 + 0x18300000 = 0x18310000`).
- **ISO Trimmed (Apenas partição de jogo / XISO)**: O descritor de volume encontra-se no setor 32 a partir do início da imagem (offset byte `32 * 2048 = 0x10000`), ou setor 0 para imagens sintéticas truncadas.
- **Magic Descriptors**: O descritor de volume é validado checando a assinatura `"MICROSOFT*XBOX*MEDIA"` (16 bytes) no início do setor e repetida no offset 2032 do setor.

### 2.2. Leitura Segura de Diretórios (BST)
- As entradas de diretório são organizadas como uma árvore binária de busca (BST) balanceada.
- Cada nó de entrada tem um cabeçalho de 14 bytes:
  - `left_child` (2 bytes little-endian, offset em palavras de 4 bytes)
  - `right_child` (2 bytes little-endian, offset em palavras de 4 bytes)
  - `starting_sector` (4 bytes little-endian)
  - `file_size` (4 bytes little-endian)
  - `attributes` (1 byte: diretório `0x10`, readonly `0x01`, etc.)
  - `name_len` (1 byte)
  - `filename` (name_len bytes, ASCII)
- **Defensas contra Entradas Hostis**:
  - `ParseDirectoryTableEntries` limita o número máximo de entradas processadas (`max_total_entries`).
  - Verificação de ciclos através de controle de setores visitados.
  - Rejeição de referências recursivas circulares e profundidade além de `max_depth` (32 níveis).
  - Verificação de que cada nó se encontra integralmente dentro do setor alocado.

### 2.3. Streaming e `SubrangeByteSource`
- Nenhuma operação lê a imagem ISO inteira na memória RAM do host.
- A classe `SubrangeByteSource` (`libs/io`) empacota uma fatia imutável do `ByteSource` pai, redirecionando offsets com checagem de limites e detecção de overflow aritmético.
- A abertura de arquivos via `XdvdfsVolume::OpenFile` retorna um `SubrangeByteSource` diretamente conectado aos setores do arquivo na imagem.

---

## 3. Camada de Abstração VFS (`libs/vfs`)

### 3.1. Normalização de Caminhos Xbox
- Suporte a caminhos no formato Xbox, como `D:\path\to\file.ext` ou `\Device\CdRom0\path`.
- Normalização canônica: converte `/` em `\`, colapsa separadores múltiplos, e garante caixa padronizada.
- **Bloqueio de Path Traversal**: Caminhos contendo `..`, caracteres de escape ou dispositivos inválidos são rejeitados com `ErrorCode::InvalidArgument`.

### 3.2. Tabela de Handles Geracionais
- `VfsHandleTable` aloca slots indexados com controle de geração de 32 bits.
- Cada fechamento de handle invalida imediatamente o par `(índice, geração)`, impedindo ataques de *use-after-free* lógico ou reaproveitamento incorreto de handles antigos (*stale handles*).
- Fechamento duplo retorna explicitamente `ErrorCode::InvalidHandle`.

### 3.3. Semântica Read-Only
- Volumes XDVDFS são montados em modo somente leitura (`IsReadOnly() == true`).
- Tentativas de criação, escrita ou exclusão retornam `ErrorCode::AccessDenied`.

---

## 4. Pipeline de Boot de Mídia (`libs/machine`)

O `MediaBootPipeline` detecta o conteúdo recebido independente da extensão do arquivo:
1. **Detecção**: Verifica se o arquivo é um executável XBE direto ou uma imagem de disco XDVDFS.
2. **Montagem**: Em caso de disco, monta o volume XDVDFS e associa-o à unidade virtual `D:`.
3. **Busca de Inicializador**: Localiza o arquivo `default.xbe` de forma case-insensitive.
4. **Carregamento Transacional**: Delega a fatia do executável (`SubrangeByteSource`) ao `XbeLoader`, que executa em duas fases (`Plan` e `Apply`).
5. **Rollback**: Qualquer falha no processo de preparação preserva ou reverte o estado anterior da sessão de máquina sem efeitos colaterais.

---

## 5. Serviços de Arquivo do Kernel HLE (`libs/kernel`)

O `KernelFileServices` expõe as chamadas de sistema NT para o ambiente convidado:
- **`NtCreateFile` / `CreateFile` (Ordinal 190)**: Abre arquivos em modo leitura no VFS a partir de ponteiros de caminho na memória guest.
- **`NtReadFile` / `ReadFile` (Ordinal 219)**: Lê dados do arquivo virtual diretamente para a memória guest de forma transacional (o ponteiro do arquivo só avança após o sucesso da escrita em guest).
- **`NtWriteFile` / `WriteFile` (Ordinal 256)**: Rejeita com `STATUS_ACCESS_DENIED` em volumes somente leitura.
- **`SetFilePointer` (Ordinal 224)**: Ajusta o ponteiro de arquivo com suporte a `FILE_BEGIN`, `FILE_CURRENT` e `FILE_END`.
- **`NtClose` (Ordinal 18)**: Libera handles do VFS de forma determinística.
- **`NtQueryInformationFile` (Ordinal 217) e `NtQueryDirectoryFile` (Ordinal 216)**: Consultam metadados e listam diretórios com formatos de estruturas guest documentados.
- **Tratamento Assíncrono**: Tentativas de I/O assíncrono com ponteiro `Overlapped` não-nulo são explicitamente rejeitadas com `STATUS_NOT_SUPPORTED` sem fingir conclusão prematura.

---

## 6. Interface Desktop e Navegador VFS (`apps/desktop`)

- **Navegador XDVDFS (`XdvdfsBrowser.tsx`)**: Componente React acessível (WCAG 2.2 AA) com navegação paginada por páginas de tamanho configurável, busca filtrada, navegação por teclado e indicação clara de arquivos e diretórios.
- **Painel de Preparação de Mídia (`MediaBootPanel.tsx`)**: Fluxo dedicado para "Preparar mídia" com avisos claros ao usuário, destacando que a preparação de mídia é distinta de execução interativa completa de jogos (*Play*).
