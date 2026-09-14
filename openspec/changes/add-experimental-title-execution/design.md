## Context

Marco 6 prepara mídia e mantém sessão/VFS; CPU e kernel são os primeiros bloqueios prováveis. Duas lanes isoladas evoluem esses subsistemas, enquanto contratos compartilhados e integração ficam nesta parent change.

## Goals / Non-Goals

**Goals:** execução real bounded, controles assíncronos, diagnóstico do primeiro bloqueio, integração CPU/kernel sem races.

**Non-Goals:** garantir boot/menu/gameplay, JIT, GPU 3D completa, áudio/input completos ou testes comerciais no repositório.

## Decisions

### Contratos congelados

Antes das lanes, definir sem editar hotspots: CPU expõe `UnsupportedInstruction` com bytes/form/EIP; kernel expõe `UnsupportedExport` com ordinal/arguments metadata; ambos usam `ExecutionStop` conceitual convertido pela integration lane. Lanes não alteram machine, C ABI, root CMake, lockfiles ou docs globais.

### Duas lanes

`cpu-execution` owns `libs/cpu/**`, testes CPU e README CPU. `kernel-services` owns `libs/kernel/**`, testes kernel e README kernel. Ambas partem do mesmo BASE_SHA e não dependem entre si.

### Integration lane

Após merge serial, integration altera machine para worker thread/join seguro e command queue; ABI C 1.5 usa handles/sized structs; Rust usa task async/cancel token; React mostra controles e snapshots. Nenhuma lógica de emulação migra ao Rust.

### Watchdog

Loop roda chunks pequenos, consulta cancel/pause em safe points e aplica quatro budgets. Snapshot usa cópia immutable sob sincronização estreita. Stop reason preserva fault arquitetural versus unsupported.

### Segurança

Logs bounded e redigidos, sem paths completos por padrão nem dump de bytes extensos. Testes E2E usam XDVDFS/XBE sintéticos. Execução real ocorre apenas localmente por ação do usuário.

## Risks / Trade-offs

- Contratos divergirem → definidos no prompt e adaptados só na integração.
- Data races → ownership único da sessão pelo worker e command queue.
- Agents tocarem hotspots → validate-ownership antes de merge.
- Hang → chunks, budgets e watchdog externo.

## Migration Plan

1. Commitar parent/child plans e regra de comunicação.
2. Criar integration branch e duas worktrees.
3. Executar/validar/merge CPU e kernel serialmente.
4. Executar integration lane em branch de integração.
5. Gates globais, diário, merge main e cleanup.