## Context
Lane paralela kernel do parent `add-experimental-title-execution`.

## Goals / Non-Goals
**Goals:** memória, timers, waits, threads, I/O e diagnostics prioritários.
**Non-Goals:** kernel completo, drivers GPU/audio/input, network ou comportamento proprietário copiado.

## Decisions
- Ownership exclusivo: `libs/kernel/**`, `tests/unit/test_kernel*`, `libs/kernel/README.md`.
- Denied: root CMake, machine, c_api, cpu, lockfiles, docs globais e parent OpenSpec.
- Serviços separados por domínio e registrados em registry existente.
- Tempo deriva somente do scheduler injetado; nunca relógio host.
- Handles geracionais e guest pointers validados antes de efeitos.
- Unsupported export metadata é tipo kernel local adaptado posteriormente pela integration lane.
- Implementar apenas semântica fundamentada em fontes públicas clean-room.

## Risks / Trade-offs
- Ordinal sem semântica confiável → permanecer unsupported.
- Wait deadlock → timeout/budgets e fila observável.
- Crescimento do dispatcher → handlers por arquivo <500 linhas.

## Testing
CTest kernel focado normal/sanitizers, invalid pointers/handles/timeouts e determinismo.