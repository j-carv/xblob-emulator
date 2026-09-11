## Purpose

Define paralelismo ACP reproduzível e sem colisões por meio de worktrees Git isolados.

## ADDED Requirements

### Requirement: Base limpa e imutável
O orquestrador SHALL criar lanes somente a partir de commit checkpoint validado, com workspace principal sem mudanças não registradas relevantes.

#### Scenario: Workspace sujo
- **WHEN** há mudanças de produto sem checkpoint
- **THEN** criação paralela é bloqueada e o usuário/orquestrador é instruído a revisar e commitá-las primeiro

### Requirement: Isolamento por worktree
Cada subagente paralelo SHALL operar em worktree e branch exclusivos, com `cwd` restrito àquele worktree e slug estável.

#### Scenario: Duas lanes
- **WHEN** CPU e GPU são independentes
- **THEN** recebem paths e branches distintos e podem executar simultaneamente

### Requirement: Escopos disjuntos
O plano SHALL declarar ownership de arquivos/áreas por lane; hotspots compartilhados MUST pertencer à lane de integração ou ser executados serialmente.

#### Scenario: ABI compartilhada
- **WHEN** duas lanes precisam editar C ABI
- **THEN** alterações ABI são retiradas das lanes ou uma depende serialmente da outra

### Requirement: Integração serial e verificável
Resultados SHALL ser revisados e integrados um por vez sem force, seguidos por resolução consciente de conflitos e suíte completa.

#### Scenario: Falha após integração
- **WHEN** merge/cherry-pick quebra teste global
- **THEN** integração para, preserva diagnóstico e não inicia push automático

### Requirement: Lifecycle seguro
Tooling SHALL validar branch/path/cleanliness e MUST NOT remover worktree com mudanças, apagar branch não integrada ou usar force por padrão.

#### Scenario: Cleanup inseguro
- **WHEN** worktree possui alterações
- **THEN** comando de remoção falha sem perda de dados

### Requirement: Registro e recuperação
Cada lane SHALL registrar change OpenSpec, task ACP, base SHA, branch, áreas owned, dependências, comandos/testes e pendências; tarefas interrompidas são retomadas no mesmo worktree.

#### Scenario: Sessão futura
- **WHEN** nova sessão lê AGENTS, manifesto e documentação
- **THEN** consegue localizar lanes e retomar task IDs sem reutilizar workspace incorreto

### Requirement: Checkpoint validado
O tooling SHALL oferecer checkpoint explícito que recusa changes OpenSpec incompletas, testes obrigatórios falhos, staging vazio ou artefatos ignoráveis, e MUST mostrar o conjunto antes de criar commit.

#### Scenario: Marco validado
- **WHEN** changes selecionadas estão completas, gates passam e arquivos são revisados
- **THEN** um commit focado pode ser criado sem push automático

### Requirement: Integration branch e ownership
O tooling SHALL criar branch `integration/<parent>` a partir do BASE_SHA comum e validar o diff de cada lane contra globs owned e denied/shared antes da integração.

#### Scenario: Lane altera hotspot
- **WHEN** o diff da lane contém arquivo fora de seus globs owned ou dentro de área shared/denied
- **THEN** a validação falha e exige integração serial consciente
