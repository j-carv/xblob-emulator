# Diário de Bordo: Sistema de Arquivos XDVDFS, VFS Virtual, Boot de Mídia e Kernel File Services HLE

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação integral da change OpenSpec `add-xdvdfs-vfs-media-boot` (Marco 6).

---

## 1. Decisões e Alinhamento Arquitetural

Neste marco, completamos a infraestrutura de I/O de disco e inicialização de mídia do **xblob**:
- **Meta do Produto**: Executar/jogar arquivos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário em seu ambiente local.
- **Estado Técnico Real**: O marco monta imagens XDVDFS reais (raw e trimmed/XISO), navega por seus diretórios de maneira bounded e segura, localiza `default.xbe`, carrega o executável via pipeline transacional com rollback e atende chamadas de serviços de arquivos do kernel. Ainda não declaramos suporte a gameplay comercial completo interativo (jogos comerciais aguardam os próximos marcos de hardware de áudio, entradas e subsistemas estendidos).
- **Conformidade Clean-Room Rigorosa**: O repositório e suas suítes de teste permanecem 100% livres de ROMs, BIOS, chaves criptográficas, headers proprietários ou jogos comerciais. Todas as validações e testes utilizam geradores programáticos sintéticos (`synthetic_media.cpp`, `synthetic_xdvdfs.cpp`).

---

## 2. Subsistemas Implementados

### 2.1. Streaming Seguro e XDVDFS (`libs/io`, `libs/formats`)
- `SubrangeByteSource`: Envelopamento seguro e imutável de janelas de bytes sobre um `ByteSource` pai com detecção de overflow e checagem de limites, eliminando a necessidade de carregar imagens ISO completas para a RAM do host.
- `XdvdfsVolume` e `ParseDirectoryTableEntries`: Parser iterativo de diretórios estruturados em árvore binária (BST), resistente a entradas malformadas, com limitação de profundidade (`max_depth = 32`), detecção de ciclos através de controle de setores visitados, e limites rígidos de contagem de entradas (`max_total_entries`).

### 2.2. Camada de Abstração de Sistema de Arquivos Virtual (`libs/vfs`)
- `Vfs`: Resolução de caminhos no padrão Xbox (unidades virtuais como `D:`, separadores canônicos de barra invertida, case-insensibilidade canônica).
- Prevenção rigorosa de path traversal: Bloqueio estrito de componentes `..`, caracteres proibidos e caminhos que tentem escapar do contêiner virtual.
- `VfsHandleTable`: Tabela de handles geracionais com tag de 32 bits, garantindo que handles reutilizados ou fechados prematuramente retornem `ErrorCode::InvalidHandle` e prevenindo ataques de use-after-free lógico.
- Volumes somente leitura: `XdvdfsVfsVolume` impõe operações estritamente de leitura, rejeitando mutações com `ErrorCode::AccessDenied`.

### 2.3. Pipeline de Boot de Mídia Transacional (`libs/machine`)
- `MediaBootPipeline`: Detecção baseada no conteúdo do arquivo (analisando cabeçalhos mágicos de XBE ou descritores XDVDFS nos setores 32 e 0), independentemente da extensão do arquivo.
- Localização case-insensitive de `default.xbe` em imagens de disco, alimentando o `XbeLoader` via `SubrangeByteSource`.
- Execução em duas fases com garantia de rollback: Caso ocorra qualquer erro em qualquer etapa (montagem, busca, parsing ou mapeamento de memória), a sessão de máquina reverte atomicamente ao estado anterior.

### 2.4. Serviços de Arquivos do Kernel HLE (`libs/kernel`)
- `KernelFileServices`: Mapeamento de chamadas de sistema NT para ordinais do kernel Xbox (`NtCreateFile` 190, `NtReadFile` 219, `NtWriteFile` 256, `SetFilePointer` 224, `NtClose` 18, `NtQueryInformationFile` 217, `NtQueryDirectoryFile` 216, `NtDeviceIoControlFile` 196).
- Leitura e busca transacionais: Validação prévia de buffers na memória do guest antes de avançar a posição do arquivo ou alterar a memória.
- I/O assíncrono: Rejeição explícita com `STATUS_NOT_SUPPORTED` em requisições com ponteiro `Overlapped` não-nulo, sem fingir conclusão assíncrona.

### 2.5. Evolução da ABI C 1.4 e Shell Desktop (`libs/c_api`, `apps/desktop`)
- ABI C 1.4: Adição das capacidades `XBLOB_CAPABILITY_XDVDFS_VFS` e `XBLOB_CAPABILITY_MEDIA_BOOT`, estruturas de relatório de boot `xblob_boot_report_t`, navegador de VFS `xblob_vfs_browser_t` com paginação two-call e listagem ordenada de entradas. Totalmente retrocompatível com as versões 1.0, 1.1, 1.2 e 1.3.
- Rust RAII e Tauri IPC: Comandos `prepare_media` e `browse_media_vfs` mantendo o shell Rust puramente como camada FFI/IPC.
- Desktop React:
  - Navegador de arquivos XDVDFS (`XdvdfsBrowser.tsx`) acessível (WCAG 2.2 AA), com paginação de tamanho variável, filtro de busca em tempo real e atalhos de teclado.
  - Painel "Preparar mídia" (`MediaBootPanel.tsx`) com fluxo em fases e alerta claro de que a preparação de mídia é distinta do estado de gameplay comercial (*Play*).
  - Modularização de `InspectionView.tsx` em subcomponentes isolados (`ValidationPanel.tsx`, `MediaBootPanel.tsx`, `XdvdfsBrowser.tsx`), mantendo todos os arquivos abaixo de 400 linhas.

---

## 3. Qualidade e Verificações Concluídas

- **Testes de Unidade e Integração**: 20/20 suítes CTest aprovadas (100% de sucesso).
- **Sanitizadores de Memória**: 20/20 suítes executadas limpas sob Clang AddressSanitizer (ASan) e UndefinedBehaviorSanitizer (UBSan).
- **Warnings-as-Errors**: Compilação sem warnings sob `-Wall -Wextra -Wpedantic -Werror` no GCC/Clang e `/W4 /WX` no MSVC.
- **Formatação de Código**: Conformidade total com `clang-format` verificada via `./tools/check-format.sh`.
- **Rust Toolchain**: `cargo fmt --check`, `cargo clippy -- -D warnings`, `cargo test --locked` (9 testes aprovados), `cargo build --locked`.
- **Frontend Web**: `npm run lint` (0 warnings), `npm run typecheck`, `vitest` (35 testes aprovados), `npm run build`, `npm run tauri -- --version`.
