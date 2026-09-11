## 1. Governança
- [x] 1.1 Atualizar `AGENTS.md` com critérios de paralelização, base limpa, parent/lane changes, ownership e limite padrão de três agentes.
- [x] 1.2 Definir arquivos compartilhados reservados à integration lane e DAG de dependências, proibindo edição concorrente sobre hotspots.
- [x] 1.3 Definir uso obrigatório de `ask_antigravity` por `cwd` de worktree, Gemini 3.8 Flash High, silêncio, task ID e retomada no mesmo path.
- [x] 1.4 Definir commits focados por lane, integração serial sem force, revisão e gates completos antes de push.

## 2. Documentação e tooling
- [x] 2.1 Criar `docs/MULTI_AGENT_WORKTREES.md` com fluxo end-to-end, layout, exemplos Pi/ACP, recuperação e troubleshooting.
- [x] 2.2 Adicionar `.worktrees/` ao gitignore e manifesto local documentado sem ignorar fontes normais.
- [x] 2.3 Criar `tools/agent-worktree.sh` com create/list/status/remove e validações de base, path, branch, cleanliness e integração.
- [x] 2.4 Criar equivalente `tools/agent-worktree.ps1` para Windows sem comandos destrutivos implícitos.
- [x] 2.5 Garantir que scripts não usem force, não removam dirty worktree/branch unmerged e suportem nomes validados contra injection/path escape.

## 3. Verificação
- [x] 3.1 Testar help/list e validação de argumentos nos scripts.
- [x] 3.2 Em repositório temporário sintético, testar create/status/remove seguro, recusa de workspace sujo e recusa de branch não integrada.
- [x] 3.3 Verificar shellcheck quando disponível e análise PowerShell quando disponível, documentando honestamente ferramentas ausentes.
- [x] 3.4 Atualizar `tools/README.md` e diário datado com estado atual: protocolo pronto, lanes reais bloqueadas até checkpoint limpo.
- [x] 3.5 Revisar portabilidade, quoting, symlink/path traversal e preservação de dados; validar `configure-parallel-acp-worktrees --strict`.

## 4. Fluxo enxuto de integração
- [x] 4.1 Adicionar comandos equivalentes POSIX/PowerShell de checkpoint explícito com preflight OpenSpec/status/gates, revisão de staging, mensagem obrigatória e sem push.
- [x] 4.2 Adicionar `init-integration` seguro para `integration/<parent>` a partir de BASE_SHA comum, recusando dirty state, nomes inválidos e divergência existente.
- [x] 4.3 Evoluir manifesto com schema/version, integration branch, owned/denied globs e dependencies, mantendo leitura retrocompatível ou migração explícita.
- [x] 4.4 Adicionar `validate-ownership` que compara diff BASE_SHA..lane, bloqueia arquivos fora de ownership/hotspots e produz relatório estável.
- [x] 4.5 Executar piloto completo de duas lanes independentes em repositório temporário: checkpoint, integration branch, DAG, ownership válido/inválido, commits, merges seriais, gates simulados e cleanup seguro.
- [x] 4.6 Atualizar AGENTS/docs/tools/diário, validar scripts com ferramentas disponíveis e executar OpenSpec strict.