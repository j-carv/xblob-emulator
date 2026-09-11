# Ferramentas e Scripts de Desenvolvimento (tools/)

Este diretório contém ferramentas auxiliares de automação, formatação e checagens de qualidade estática para o projeto **xblob**.

## Scripts e Utilitários

- `check-format.sh`: Script para executar `clang-format` recursivamente em modo de verificação nas árvores `apps/`, `libs/` e `tests/`.
- `generate-fixtures.py`: Utilitário para gerar fixtures sintéticas de teste quando necessário fora da suíte CTest.
- `agent-worktree.sh`: Script POSIX para automação do ciclo de vida de Git worktrees isolados para agentes paralelos ACP. Suporta `create`, `checkpoint`, `init-integration`, `validate-ownership`, `set-lane-meta`, `list`, `status`, `set-task`, `remove` e `help`.
- `agent-worktree.ps1`: Implementação equivalente em PowerShell para Windows sem comandos destrutivos implícitos.

### Subcomandos Disponíveis

- `create <lane> [base-commit] [--parent <parent>] [--change <change>] [--owned <globs>] [--denied <globs>] [--depends-on <lanes>]`: Cria o worktree isolado em `.worktrees/<lane>` e registra metadados no manifesto local.
- `checkpoint -m "<mensagem>" [--change <change>] [--gate-cmd "<cmd>"]`: Executa commit explícito dos arquivos em staging com preflight OpenSpec strict e gates configurados. Recusa staging vazio e nunca faz push.
- `init-integration <parent> [base-commit]`: Cria com segurança a branch de integração `integration/<parent>` a partir de uma base comum e limpa.
- `validate-ownership <lane> [--base <commit>] [--json]`: Inspeciona arquivos alterados no diff da lane contra o commit base, bloqueando edições fora de `owned` ou que toquem em hotspots protegidos globais.
- `set-lane-meta <lane> [--change <change>] [--owned <globs>] [--denied <globs>] [--depends-on <lanes>]`: Atualiza metadados e dependências no manifesto local.
- `set-task <lane> <task-id>`: Registra o identificador assíncrono de tarefa ACP para rastreabilidade.
- `list [--json]`: Lista worktrees git ativos e estado do manifesto `.worktrees/manifest.json` (schema 2.0).
- `status [lane]`: Detalha cleanliness, branch, commit e status de integração (HEAD e integration branch).
- `remove <lane> [--delete-branch]`: Remove o worktree de forma segura (sem force), recusando remoção caso haja arquivos dirty ou commits não integrados.

### Regras de Segurança dos Scripts de Worktree

1. **Sem Force**: Nenhum script utiliza `--force`, `-f` ou `git reset --hard`.
2. **Exigência de Base Limpa**: Os comandos `create` e `init-integration` bloqueiam a execução se o repositório principal possuir modificações não commitadas (`git status --porcelain`).
3. **Preservação de Dados**: O comando `remove` recusa a remoção caso o worktree possua edições locais não salvas ou caso a branch possua commits não integrados na árvore principal ou branch de integração.
4. **Proteção contra Injeção e Path Traversal**: Identificadores de lane e parent são estritamente validados contra regex `^[a-zA-Z0-9_-]+$`, e caminhos canônicos são verificados contra a raiz de `.worktrees/`.
5. **Governança de Hotspots e Ownership**: `validate-ownership` protege áreas críticas (`CMakeLists.txt`, `libs/c_api/**`, `libs/machine/**`, lockfiles, docs globais) contra alterações diretas por agentes em lanes paralelas.


### Estado Atual e Bloqueio Operacional

O protocolo e ferramental de orquestração paralela estão prontos e validados. Contudo, a criação de lanes reais no repositório atual permanece **bloqueada** até que as alterações pendentes dos Marcos 5/6 sejam revisadas, validadas e formalmente commitadas na branch principal.
