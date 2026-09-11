# Diário de Bordo: Protocolo de Orquestração Paralela de Agentes com Git Worktrees

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação integral da change OpenSpec `configure-parallel-acp-worktrees`.

---

## 1. Contexto e Decisões Tomadas

Para acelerar o desenvolvimento do emulador **xblob** nos próximos marcos de engenharia (onde subsistemas independentes como CPU, GPU, Kernel HLE, Áudio e UI podem avançar em paralelo), configuramos um fluxo seguro de execução paralela de agentes de IA baseado no protocolo ACP (*Agent Control Protocol*) e Git Worktrees:

1. **Evitar Gargalos de Workspace Único**:
   - Múltiplas invocações do assistente no mesmo diretório de trabalho são serializadas pela extensão e geram risco extremo de colisões destrutivas de arquivos.
   - O uso de Git Worktrees aloca diretórios de trabalho independentes vinculados ao mesmo repositório local, proporcionando isolamento total de `cwd` e branch.
2. **Requisito de Base Limpa e Imutável (`BASE_SHA`)**:
   - Nenhuma lane paralela pode ser criada sobre um workspace sujo. Todas as lanes devem derivar estritamente de um commit checkpoint auditado.
3. **Limite de Concorrência e Escalonamento**:
   - Fixado o limite mandatório de **no máximo 3 lanes concorrentes simultâneas** para garantir a estabilidade do host (CPU, memória e I/O).
4. **Ownership Disjunto e Hotspots Reservados**:
   - Cada lane atua apenas em arquivos do seu domínio.
   - Arquivos estruturais e centrais (`CMakeLists.txt` raiz, C ABI em `libs/c_api/`, orquestração em `libs/machine/`, lockfiles e documentação global) são reservados com exclusividade à **integration lane** ou tratados em etapas seriais.
5. **Invocação ACP Silenciosa com Gemini 3.8 Flash High**:
   - Subagentes executam com `ask_antigravity` com `cwd` apontando para `.worktrees/<lane>`, modelo Gemini 3.8 Flash High, operando em modo estritamente silencioso e registrando task IDs no manifesto local `.worktrees/manifest.json`.
6. **Integração Serial e Gates Completos**:
   - Integração serial sem avanço rápido (`merge --no-ff`) ou cherry-pick, sem uso de `--force`, seguida por validação dos gates de teste completos.

---

## 2. Progresso Realizado

1. **Governança (`AGENTS.md`)**:
   - Adicionada a Seção 9 definindo formalmente o protocolo normativo para futuras sessões: critérios de paralelização, parent/lane changes, base SHA limpa, ownership disjunto, limite de até 3 lanes, hotspots reservados, uso mandatório de `ask_antigravity` por `cwd` exclusivo com Gemini 3.8 Flash High, registro de task IDs, commits focados por lane e integração serial sem force.
2. **Documentação de Arquitetura e Operação (`docs/MULTI_AGENT_WORKTREES.md`)**:
   - Criado guia completo contendo fluxo end-to-end, layout de diretórios, convenções de branches, formato do manifesto JSON, exemplos conceituais de chamada ACP, ciclo de vida de uma lane, recuperação de sessões interrompidas e tabela de troubleshooting.
3. **Isolamento no Git (`.gitignore`)**:
   - Adicionada a entrada `.worktrees/` garantindo que os diretórios de trabalho paralelos e o manifesto local não sejam comitados na árvore principal.
4. **Ferramental Automatizado (`tools/agent-worktree.sh` e `tools/agent-worktree.ps1`)**:
   - Criados scripts compatíveis POSIX e PowerShell com os comandos `create`, `list`, `status`, `set-task`, `remove` e `help`.
   - Implementadas validações defensivas:
     - Rejeição de identificadores inválidos via regex `^[a-zA-Z0-9_-]+$`;
     - Prevenção de fuga de diretório (*symlink escape* / *path traversal*);
     - Bloqueio de criação se o workspace principal contiver alterações não salvas (dirty);
     - Bloqueio de remoção se o worktree contiver arquivos modificados/untracked (dirty);
     - Bloqueio de remoção se a branch contiver commits não integrados na árvore principal;
     - Proibição absoluta de `--force` ou deleções destrutivas implícitas.
5. **Documentação de Tooling (`tools/README.md`)**:
   - Atualizado para descrever os novos scripts, suas regras de segurança e o estado operacional atual.

---

## 3. Comandos Executados e Resultados de Testes

1. **Validação de Sintaxe e Argumentos**:
   - `bash -n tools/agent-worktree.sh`: Aprovado sem erros.
   - `./tools/agent-worktree.sh help`: Aprovado, exibe ajuda formatada e opções.
   - `./tools/agent-worktree.sh list`: Aprovado, lista worktrees Git ativos.
   - `./tools/agent-worktree.sh create "../bad"`: Aprovado, rejeita path traversal / caracteres inválidos com código 1.
   - `./tools/agent-worktree.sh create cpu`: Aprovado, detecta workspace sujo no repositório atual e recusa a criação com mensagem instrutiva.
2. **Teste de Ciclo de Vida em Repositório Temporário Sintético**:
   - Criado repositório Git isolado em diretório temporário (`/var/folders/.../test-xblob-wt-*`).
   - Executada a criação de lane (`create lane1 HEAD --parent marco-7`): gerou `.worktrees/lane1`, branch `agent/marco-7/lane1` e inicializou `.worktrees/manifest.json`.
   - Testada a listagem (`list`) e status (`status lane1`): confirmou estado limpo e base SHA.
   - Testado o registro de task (`set-task lane1 task-9876`): atualizou o manifesto com sucesso.
   - Testada a recusa de criação com workspace sujo: arquivo não commitado no repositório principal causou recusa imediata.
   - Testada a recusa de remoção com worktree sujo: edição não commitada no worktree impediu remoção (sem force).
   - Testada a recusa de remoção com branch não integrada: commit adicionado à branch da lane impediu remoção.
   - Realizado merge da branch na main e testada a remoção segura (`remove lane1 --delete-branch`): worktree e branch removidos com sucesso, manifesto atualizado.
3. **Análise Estática e Ferramentas**:
   - Verificada a disponibilidade de ferramentas adicionais via `which shellcheck pwsh powershell`.
   - Constatou-se honestamente que `shellcheck`, `pwsh` e `powershell` **não estão instalados** no ambiente de execução (macOS host). A verificação de sintaxe Bash foi realizada com `bash -n`.

---

## 4. Estado Atual e Limitações Conhecidas

- **Lanes Reais Bloqueadas no Repositório Atual**: O repositório contém alterações substanciais dos incrementos anteriores (Marcos 5 e 6: XDVDFS, VFS virtual, boot de mídia, kernel file services e React desktop foundation). Conforme o protocolo de segurança, nenhuma lane real foi criada no repositório atual para não violar a integridade da árvore de trabalho suja.

---

## 5. Implementação da Fase 4: Checkpoint, Integração e Governança de Ownership

### 5.1. Novas Capacidades Implementadas
1. **Checkpoint Explícito e Seguro (`checkpoint`)**:
   - Subcomando adicionado a `tools/agent-worktree.sh` e `tools/agent-worktree.ps1`.
   - Exige mensagem de commit mandatória (`-m`/`-Message`).
   - Valida staged files com `diff --cached --name-status`.
   - Bloqueia auto-staging indiscriminado (`git add -A` vedado; staging vazio é rejeitado).
   - Executa preflight obrigatório OpenSpec strict para changes afetadas e preflight de gates de qualidade (`--gate-cmd` ou `./tools/check-format.sh`). Não possui bypass de gates.
   - NUNCA executa push automático para remotos.
2. **Inicialização da Branch de Integração (`init-integration`)**:
   - Cria `integration/<parent>` a partir de `BASE_SHA` limpo e verificado.
   - Recusa execução se o workspace principal contiver alterações não commitadas (dirty).
   - Rejeita divergência caso a branch de integração já exista apontando para outro commit.
3. **Manifesto Local Versionado (Schema 2.0)**:
   - Migração e retrocompatibilidade automáticas para `schemaVersion: "2.0"`.
   - Rastreia `parent`, `integrationBranch`, `baseSha` no nível raiz.
   - Rastreia por lane: `lane`, `parent`, `branch`, `path`, `baseSha`, `createdAt`, `taskId`, `change`, `owned`, `denied`, `dependencies`.
   - Novo comando `set-lane-meta` para configuração fina de escopo e DAG.
4. **Validação Estrita de Ownership (`validate-ownership`)**:
   - Inspeciona arquivos modificados no diff `BASE_SHA..branch` e no worktree local da lane.
   - Aplica lista de hotspots globais protegidos (`CMakeLists.txt`, `libs/c_api/**`, `libs/machine/**`, lockfiles, docs globais) somados a `denied` globs.
   - Exige que todo arquivo alterado coincida com ao menos um padrão `owned` da lane (quando declarado).
   - Produz relatório legível ou JSON estruturado (`--json`) e encerra com código 1 caso haja violação.

### 5.2. Resultados do Piloto Automatizado em Repositório Sintético Temporário (`tests/test_agent_worktree_pilot.sh`)
- Repositório temporário criado e isolado em `/var/folders/.../xblob-pilot-*`.
- **Checkpoint**:
  - Tentativa sem mensagem: bloqueada com erro.
  - Tentativa com staging vazio: bloqueada (sem auto-stage).
  - Tentativa com gate preflight falho (`exit 1`): abortada com erro.
  - Checkpoint válido com gate aprovado: commit criado com sucesso, push não executado.
- **Init-Integration**:
  - Tentativa com workspace dirty: bloqueada.
  - Tentativa com branch divergente: bloqueada.
  - Inicialização válida: `integration/marco-pilot` criada em `BASE_SHA`.
- **Lanes Paralelas e DAG**:
  - Criadas duas lanes concorrentes: `cpu-lane` (`owned: libs/cpu/**`) e `gpu-lane` (`owned: libs/gpu/**`).
  - Metadados registrados em `.worktrees/manifest.json` com schema 2.0.
- **Validação de Ownership**:
  - Edição válida em `libs/cpu/cpu.cpp`: `validate-ownership cpu-lane` -> APROVADO.
  - Violação injetada em `CMakeLists.txt`: `validate-ownership cpu-lane` -> REJEITADO (bloqueio imediato com relatório de violação de hotspot).
  - Reversão da violação: `validate-ownership cpu-lane` -> APROVADO.
  - Edição válida em `libs/gpu/gpu.cpp`: `validate-ownership gpu-lane` -> APROVADO.
- **Integração Serial e Gates**:
  - Merge serial `--no-ff` de `cpu-lane` em `integration/marco-pilot`, com gate simulado aprovado.
  - Merge serial `--no-ff` de `gpu-lane` em `integration/marco-pilot`, com gate simulado aprovado.
  - Integração final da branch de integração na `main`.
- **Remoção Segura sem Force**:
  - Tentativa de remoção com worktree dirty: bloqueada sem perda de dados.
  - Tentativa de remoção de branch com commits não integrados: bloqueada sem perda de dados.
  - Remoção de lanes integradas com `--delete-branch`: concluída com sucesso sem force, atualizando manifesto.
- Repositório sintético temporário descartado de forma limpa.

### 5.3. Disponibilidade de Ferramentas no Host
- `bash -n`: Executado com sucesso em `tools/agent-worktree.sh` e `tests/test_agent_worktree_pilot.sh`.
- `python3`: Presente e utilizado para manipulação robusta de JSON e matching de caminhos/globs.
- `shellcheck`, `pwsh`, `powershell`, `PSScriptAnalyzer`: **Ausentes/não instalados** no ambiente macOS host. Sua ausência é documentada honestamente. A sintaxe de `tools/agent-worktree.ps1` foi elaborada de acordo com as especificações padrão do PowerShell 7/Core.

