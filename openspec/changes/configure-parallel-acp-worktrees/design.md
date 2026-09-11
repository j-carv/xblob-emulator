## Context

`ask_antigravity` aceita `cwd` e enfileira tarefas por workspace. Assim, várias chamadas no mesmo diretório não são paralelas; worktrees distintos evitam fila e colisão. O repositório atualmente possui mudanças pós-commit, portanto esta change configura o fluxo sem abrir lanes reais.

## Goals / Non-Goals

**Goals:** protocolo persistente; worktrees/branches previsíveis; paralelismo apenas seguro; scripts sem force; integração/testes auditáveis; recuperação entre sessões.

**Non-Goals:** scheduler ACP customizado, merge automático irrestrito, force-push, aprovação automática destrutiva ou paralelizar mudanças acopladas.

## Decisions

### Parent change e lane changes

Um marco paralelo terá uma change pai de integração e child changes/planos por lane com critérios e ownership. Sugestão: CPU, kernel, GPU, audio/input e UI. Shared contracts são definidos antes; root CMake, C ABI, machine composition, lockfiles e docs globais ficam na integration lane salvo ownership exclusivo.

### Layout

Worktrees locais usam `.worktrees/<lane>` (ignorado) e branches `agent/<parent>/<lane>`, sempre derivadas de `BASE_SHA` comum validado. O task ACP recebe o worktree como `cwd`, modelo `gemini-3.8-flash-high`, instrução silenciosa e paths permitidos.

### Limite de concorrência

Padrão de até 3 lanes simultâneas; aumentar somente após observar CPU/RAM e filas. Dependências entre lanes formam DAG: somente nós sem dependências rodam juntos.

### Commit e integração

Cada lane produz commits focados depois dos testes próprios. Um checkpoint explícito executa preflight configurável (OpenSpec completo, status, staged review e gates), cria commit somente com mensagem informada e nunca faz push. A branch `integration/<parent>` nasce do BASE_SHA comum. A integration lane revisa diff/relatório e faz merge `--no-ff` ou cherry-pick serial, nunca force. Após cada integração executa checks afetados; ao final roda CTest normal/sanitizers, Rust, frontend e OpenSpec strict.

### Tooling

Manter scripts POSIX e PowerShell equivalentes com `checkpoint`, `init-integration`, `create`, `list`, `status`, `validate-ownership` e `remove`. `checkpoint` exige revisão e gates; `init-integration` recusa branch divergente; `create` exige main worktree clean e commit existente; `remove` recusa dirty/unmerged. Scripts apenas gerenciam Git; disparo ACP continua pelo orquestrador para preservar auditoria/permissões.

### Persistência

Manifesto local ignorado em `.worktrees/manifest.json` registra parent, integration branch, base SHA, lane, branch, path, task ID, globs owned/denied e dependências quando disponíveis. Estado autoritativo continua Git + OpenSpec + registro ACP. Documentação explica retomada no mesmo `cwd`.

## Risks / Trade-offs

- Contratos mudam em paralelo → freeze inicial e integration ownership.
- Muitos agents saturam host → limite 3.
- Commits de lane incluem arquivos alheios → allowlist + diff review.
- Cleanup perde dados → sem `--force`, clean/unmerged checks.
- Workspace sujo atual → checkpoint separado antes da primeira execução real.

## Migration Plan

1. Atualizar AGENTS e documentação.
2. Adicionar scripts POSIX/PowerShell e ignores.
3. Testar help/list e recusas em workspace sujo usando dry-run/temp repo.
4. Validar checkpoint/integration/ownership e duas lanes em repositório temporário.
5. Validar OpenSpec; criar lanes reais somente após checkpoint dos marcos atuais.