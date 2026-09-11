## 1. I/O e XDVDFS
- [x] 1.1 Implementar `SubrangeByteSource` imutável com base/tamanho/overflow checked e testes de EOF/lifetime.
- [x] 1.2 Definir tipos XDVDFS de volume/entry/path e budgets sem structs sobre buffers; documentar offsets/fontes públicas.
- [x] 1.3 Implementar mount raw/trimmed validando descriptor, sector math, root range e tamanho da fonte.
- [x] 1.4 Implementar traversal iterativo de diretórios com visited/depth/node/name budgets e detecção de ciclos/duplicatas.
- [x] 1.5 Implementar lookup case-insensitive e leitura/view bounded de arquivos, incluindo EOF e setores truncados.
- [x] 1.6 Criar fixtures XDVDFS programáticas nested/case/raw/trimmed e malformed para todos os limites.

## 2. VFS
- [x] 2.1 Criar `xblob_vfs`, contratos de volume/node e dependency graph sem host paths.
- [x] 2.2 Implementar normalização Xbox, aliases/drive mounts e bloqueio de traversal/escape/componentes inválidos.
- [x] 2.3 Implementar handle table geracional tipada com open/close e stale/double-close tests.
- [x] 2.4 Implementar read/seek/query/enumerate read-only com commit de posição após sucesso e status estruturados.
- [x] 2.5 Verificar isolamento/teardown de duas namespaces e rejeição explícita de create/write/delete.

## 3. Boot de mídia
- [x] 3.1 Criar detector/pipeline por conteúdo para XBE direto e XDVDFS ISO/XISO, independente de extensão.
- [x] 3.2 Localizar `default.xbe` case-insensitivamente e entregar SubrangeByteSource ao loader sem arquivo temporário.
- [x] 3.3 Implementar estágios detect/mount/lookup/parse/plan/apply e rollback preservando sessão anterior.
- [x] 3.4 Integrar mount/source ownership à MachineSession sem god object e com lifecycle determinístico.
- [x] 3.5 Testar E2E XBE direto e raw/trimmed ISO→default.xbe→Prepared, default ausente e XBE corrompido.

## 4. Kernel file HLE
- [x] 4.1 Adicionar módulo file_services com ABI guest/structs/status documentados e registry clean-room.
- [x] 4.2 Implementar open/create-readonly e close com argumentos/strings guest checked.
- [x] 4.3 Implementar read e seek transacionais validando todo destino antes de posição/cópia.
- [x] 4.4 Implementar query file e directory enumeration bounded com layouts guest explícitos.
- [x] 4.5 Mapear not-found/invalid-handle/access/EOF/parameter/unsupported e rejeitar async sem fingir completion.
- [x] 4.6 Testar file HLE por programa guest sintético incluindo páginas inválidas, stale handles e duas threads.

## 5. ABI e desktop
- [x] 5.1 Evoluir ABI C para 1.4 com capabilities, boot report e directory pagination sized/two-call, preservando 1.0–1.3.
- [x] 5.2 Implementar Rust RAII/IPC para browse/prepare com cancelamento e sem parsing/lógica VFS.
- [x] 5.3 Implementar árvore/lista XDVDFS acessível, paginada e responsiva com seleção de default.xbe e estados loading/error.
- [x] 5.4 Implementar ação “Preparar mídia” e progresso por estágio, distinguindo preparação de jogabilidade.
- [x] 5.5 Testar bridges mock/real, keyboard/focus/axe, mídia grande, cancelamento e erros por estágio.

## 6. Qualidade e documentação
- [x] 6.1 Atualizar CMake install/export e CI macOS/Linux/Windows, sem duplicate-link warnings.
- [x] 6.2 Executar warnings-as-errors, format, clangd, CTest normal e ASan+UBSan com entradas hostis.
- [x] 6.3 Executar C11 ABI legacy smoke, npm ci/lint/typecheck/test/build/Tauri CLI e cargo fmt/clippy/test/build locked.
- [x] 6.4 Auditar streaming/memória, ownership, path escape, atomicidade, >500 linhas, CSP e conteúdo proprietário.
- [x] 6.5 Atualizar README, ARCHITECTURE, VFS/XDVDFS/kernel docs, proveniência e diário com estado honesto.
- [x] 6.6 Validar `add-xdvdfs-vfs-media-boot --strict` e marcar apenas critérios reproduzíveis.