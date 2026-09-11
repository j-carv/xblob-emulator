## Why

Marcos futuros podem avançar CPU, kernel, GPU, áudio e UI em paralelo, mas agentes no mesmo workspace são serializados pela extensão e alterações concorrentes colidem. Worktrees Git isolados permitem paralelismo real desde que a base esteja commitada, os escopos sejam disjuntos e a integração permaneça controlada.

## What Changes

- Definir em `AGENTS.md` o protocolo normativo de decomposição, worktrees, branches, ACP e integração.
- Documentar papéis de lane, arquivos reservados, critérios de paralelização, permissões, checkpoints e recuperação.
- Adicionar tooling portátil razoável para criar/listar/remover worktrees sem operações destrutivas implícitas.
- Reservar CMake raiz, ABI, machine composition, lockfiles e documentação global para uma lane de integração quando houver sobreposição.
- Exigir testes por lane e suíte completa após integração.

## Capabilities

### New Capabilities
- `parallel-agent-orchestration`: Execução ACP paralela segura em worktrees e integração serial auditável.

### Modified Capabilities
_Nenhuma._

## Impact

Altera somente governança, documentação e tooling de desenvolvimento. Não cria worktrees automaticamente sobre workspace sujo, não altera código do emulador e não executa merge/commit/push sem checkpoint explícito.