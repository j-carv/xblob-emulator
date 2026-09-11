## Context

Formats detecta XDVDFS, loader aceita ByteSource e machine prepara XBE, mas não há árvore de disco, VFS ou file HLE. A solução deve funcionar sobre arquivos grandes sem cópia integral e manter mídia do usuário local/read-only.

## Goals / Non-Goals

**Goals:** parser XDVDFS completo e bounded; VFS Xbox read-only; default.xbe por view; serviços HLE síncronos; árvore e preparação na UI.

**Non-Goals deste marco:** FATX/HDD gravável, save games, I/O assíncrono completo, DVD streaming preciso, patches/keys, garantia de gameplay ou armazenamento de mídia no projeto.

## Decisions

### XDVDFS em formats, VFS separado

Ampliar `libs/formats` com `XdvdfsVolume`, entry parser e iterator sobre `ByteSource`. Criar `libs/vfs` dependente de common+io+formats para namespace/mounts/handles. Kernel depende da abstração VFS, não de XDVDFS ou host filesystem diretamente.

### Views bounded

Adicionar `SubrangeByteSource` em io com base/tamanho checked. Arquivos XDVDFS retornam view imutável; loader lê `default.xbe` sem extração temporária. Lifetime é explícito por shared immutable source/session ownership.

### Parser iterativo e com budgets

Entradas são decodificadas campo a campo, sem struct casts. Traversal usa stack/visited set, limites configurados de depth/nodes/name/total metadata e valida offset relativo antes de multiplicar por setor. Nomes normalizados em ASCII case-fold documentado; bytes inválidos geram erro conservador.

### VFS e handles

`VfsNamespace` resolve aliases/drive e componentes canônicos. `HandleTable` usa índice+generation e variant de file/directory. Position updates ocorrem somente após read/copy bem-sucedidos. Mount D: é read-only; nenhum path vira path host.

### Boot em estágios

`MediaBootPipeline` detecta conteúdo, cria source (direct XBE ou XDVDFS entry), valida/planeja loader e só substitui sessão ao final. `BootDiagnostic` inclui stage, source type, executable path e erro. ISO raw offset é encapsulado pelo volume.

### Kernel HLE

Serviços ficam em módulo `kernel/file_services`, usando guest-memory accessors transacionais. Nomes/ordinais seguem documentação pública clean-room; formas não implementadas retornam unsupported. I/O inicial é síncrono e bounded por chamada.

### ABI/UI

ABI 1.4 expõe handles opacos de tree/boot report, paginação de directory entries e strings two-call. Rust traduz DTOs apenas. React mostra árvore virtualizada/paginada e ação “Preparar mídia”; não lê filesystem diretamente nem habilita Play antes de execução real validada.

### Qualidade

Gerar imagens XDVDFS pequenas programaticamente cobrindo raw/trimmed, nested/case, malformed/cycles/overflow. Testar E2E ISO→default.xbe→session e file HLE. CTest normal/sanitizers, C11 ABI, Rust/React e três hosts.

## Risks / Trade-offs

- Especificação XDVDFS incompleta → referências públicas, fixtures focadas e unsupported conservador.
- Imagens multi-GB → streaming/subrange e budgets, nunca read-all.
- Lifetime de source → ownership imutável preso ao mount/session.
- Parser recursivo hostil → traversal iterativo e limites.
- UI sugerir jogabilidade → separar preparar de jogar e mostrar estágio/capacidades.

## Migration Plan

1. Subrange source e parser XDVDFS.
2. VFS/mount/path/handles.
3. Boot pipeline e machine ownership.
4. File HLE.
5. ABI 1.4 e desktop tree/preparation.
6. CI, fuzz-like malformed tests, sanitizers e documentação.