# Orquestração Paralela de Agentes com Git Worktrees (docs/MULTI_AGENT_WORKTREES.md)

Este documento descreve o fluxo de trabalho ponta a ponta (*end-to-end*), a arquitetura de isolamento, a governança de branches e a integração de tarefas com múltiplos agentes autônomos executando em paralelo sobre o repositório **xblob** através do protocolo ACP (*Agent Control Protocol*) e Git Worktrees.

---

## 1. Contexto e Motivação

No desenvolvimento acelerado do emulador **xblob**, múltiplos subsistemas independentes (tais como CPU, Kernel HLE, GPU NV2A, Barramento PCI, Áudio e Interface de Usuário) podem avançar concomitantemente.

Entretanto, executar múltiplos agentes de IA no **mesmo workspace de trabalho** gera problemas críticos:
1. **Serialização forçada**: A extensão de IA e o runtime do editor serializam chamadas enfileiradas no mesmo diretório de trabalho (`cwd`).
2. **Colisões destrutivas de arquivos**: Dois agentes editando simultaneamente o mesmo arquivo ou arquivos interdependentes sobrescrevem alterações de forma não determinística.
3. **Contaminação de contexto e cache**: Compilações parciais, caches de ferramentas e logs misturados degradam a capacidade de diagnóstico.

A utilização de **Git Worktrees** resolve estes gargalos permitindo múltiplos diretórios de trabalho independentes vinculados ao mesmo repositório local, onde cada agente atua isolado em seu próprio `cwd` e branch, mantendo uma base de código compartilhada e um histórico limpo e auditável.

---

## 2. Princípios e Requisitos Mandatórios

Conforme definido na Seção 9 de [`AGENTS.md`](../AGENTS.md#L127-L188):

1. **Base Limpa e Imutável (`BASE_SHA`)**:
   - Worktrees paralelos DEVEM ser criados estritamente a partir de um commit válido e estável (`BASE_SHA`).
   - O workspace principal DEVE estar limpo (`git status --porcelain` vazio). A criação de lanes sobre um workspace com arquivos modificados ou untracked não commitados é **estritamente bloqueada**.
2. **Limite de Concorrência**:
   - Padrão mandatório de **no máximo 3 lanes paralelas simultâneas** para preservar estabilidade de host (CPU, memória RAM e I/O de disco) e prevenir saturação de contexto.
3. **Ownership Estrito e Disjunto**:
   - Cada lane opera exclusivamente em seus arquivos atribuídos (ex.: `libs/cpu/`, `libs/kernel/`, etc.).
   - Modificações cruzadas de arquivos fora do escopo da lane são expressamente proibidas.
4. **Hotspots Reservados à Integration Lane**:
   - Arquivos globais de coordenação e contratos centrais JAMAIS sofrem edição concorrente por lanes paralelas:
     - CMake raiz: [`CMakeLists.txt`](../CMakeLists.txt)
     - C ABI pública e FFI: [`libs/c_api/include/xblob/c_api.h`](../libs/c_api/include/xblob/c_api.h), [`libs/c_api/src/c_api.cpp`](../libs/c_api/src/c_api.cpp)
     - Orquestração de máquina: [`libs/machine/include/xblob/machine/machine_session.hpp`](../libs/machine/include/xblob/machine/machine_session.hpp), [`libs/machine/src/machine_session.cpp`](../libs/machine/src/machine_session.cpp)
     - Dependências e lockfiles: [`apps/desktop/package-lock.json`](../apps/desktop/package-lock.json), `Cargo.lock`
     - Governança e documentação global: [`AGENTS.md`](../AGENTS.md), [`README.md`](../README.md), [`ARCHITECTURE.md`](../ARCHITECTURE.md)
5. **Comunicação Silenciosa e Modelo de Alta Precisão**:
   - Subagentes paralelos utilizam o modelo **Gemini 3.8 Flash High** com instrução mandatória de trabalho silencioso (sem narrativa intermediária).
6. **Integração Serial sem Force**:
   - A integração na branch principal ocorre de forma estritamente serial (uma lane por vez), precedida por revisão de diff e testes locais. O uso de `--force`, `-f` ou `git reset --hard` é expressamente vedado.

---

## 3. Layout do Sistema de Arquivos e Convenções

```
xblob/
├── .git/                      # Repositório Git central compartilhado
├── .worktrees/                # Diretório ignorado contendo os worktrees locais
│   ├── manifest.json          # Manifesto local ignorado rastreando lanes ativas
│   ├── cpu/                   # Worktree isolado da lane 'cpu' (branch agent/<parent>/cpu)
│   ├── kernel/                # Worktree isolado da lane 'kernel' (branch agent/<parent>/kernel)
│   └── gpu/                   # Worktree isolado da lane 'gpu' (branch agent/<parent>/gpu)
├── AGENTS.md
├── CMakeLists.txt
├── apps/
├── libs/
├── tests/
└── tools/
    ├── agent-worktree.sh      # Tooling POSIX para gestão do ciclo de vida
    └── agent-worktree.ps1     # Tooling PowerShell para Windows
```

### 3.1. Nomenclatura Padrão
- **Diretório do Worktree**: `.worktrees/<lane>` (ex.: `.worktrees/cpu`)
- **Branch Git**: `agent/<parent-change>/<lane>` (ex.: `agent/milestone-7/cpu`) ou `agent/<lane>`
- **Manifesto de Orquestração**: `.worktrees/manifest.json` (rastreia lane, parent, branch, path, baseSha, createdAt, taskId)

#### 3.2. Formato do Manifesto Local (`.worktrees/manifest.json` - Schema 2.0)
```json
{
  "schemaVersion": "2.0",
  "version": "2.0",
  "parent": "m7-core-execution",
  "integrationBranch": "integration/m7-core-execution",
  "baseSha": "e4a7b218f3a0...",
  "worktrees": [
    {
      "lane": "cpu-decoder",
      "parent": "m7-core-execution",
      "branch": "agent/m7-core-execution/cpu-decoder",
      "path": ".worktrees/cpu-decoder",
      "baseSha": "e4a7b218f3a0...",
      "createdAt": "2026-09-11T20:00:00Z",
      "taskId": "task-8f92a1c0",
      "change": "feat-cpu-decoder",
      "owned": [
        "libs/cpu/**",
        "tests/unit/test_cpu*"
      ],
      "denied": [
        "CMakeLists.txt",
        "libs/c_api/**",
        "libs/machine/**"
      ],
      "dependencies": []
    }
  ]
}
```

---

## 4. Orquestração e Exemplo com ACP (`ask_antigravity`)

O orquestrador principal (humano ou agente líder) decompõe o marco em uma change pai e sub-changes/planos por lane. Para disparar um agente paralelo em seu worktree exclusivo:

```typescript
// Exemplo conceitual de despacho via ask_antigravity
await ask_antigravity({
  cwd: "/caminho/absoluto/xblob/.worktrees/cpu-decoder",
  prompt: `
Você está atuando na lane 'cpu-decoder' do marco 'm7-core-execution'.
Seu repositório de trabalho é estritamente este worktree.
Modelo obrigatório: Gemini 3.8 Flash High.

Objetivo: Implementar decodificação das instruções x86 SSE2 em libs/cpu/.
Arquivos de escopo exclusivo: libs/cpu/include/xblob/cpu/*, libs/cpu/src/*, tests/unit/test_cpu*.cpp.
Proibido alterar: CMakeLists.txt raiz, libs/c_api/, libs/machine/, AGENTS.md.

Trabalhe silenciosamente: sem narrativa intermediária, sem comentários de progresso.
Ao concluir, execute os testes unitários da sua lane (ctest -R test_cpu) e responda exatamente com:
## Resumo
## Arquivos alterados
## Comandos e testes executados
## Falhas ou riscos restantes
`
});
```

---

## 5. Ciclo de Vida de uma Lane de Trabalho

### Passo 1: Inicialização da Branch de Integração
Antes de criar as lanes paralelas, inicialize a branch de integração comum a partir do `BASE_SHA` limpo:
```bash
git status
# Deve retornar: "nothing to commit, working tree clean"

./tools/agent-worktree.sh init-integration m7-core-execution HEAD
```
*No Windows (PowerShell):*
```powershell
.\tools\agent-worktree.ps1 init-integration m7-core-execution HEAD
```

### Passo 2: Criação do Worktree com Ownership e Dependências
Crie o worktree declarando escopo de arquivos e dependências no DAG:
```bash
./tools/agent-worktree.sh create cpu-decoder HEAD \
  --parent m7-core-execution \
  --change feat-cpu-decoder \
  --owned "libs/cpu/**,tests/unit/test_cpu*" \
  --denied "CMakeLists.txt,libs/c_api/**,libs/machine/**"
```
*No Windows (PowerShell):*
```powershell
.\tools\agent-worktree.ps1 create cpu-decoder HEAD `
  -Parent m7-core-execution `
  -Change feat-cpu-decoder `
  -Owned "libs/cpu/**,tests/unit/test_cpu*" `
  -Denied "CMakeLists.txt,libs/c_api/**,libs/machine/**"
```

### Passo 3: Registro do Task ID no Manifesto
Caso o despacho do agente gere um identificador assíncrono de tarefa:
```bash
./tools/agent-worktree.sh set-task cpu-decoder "task-8f92a1c0"
```

### Passo 4: Execução, Testes e Checkpoint Explícito da Lane
O agente atua dentro de `.worktrees/cpu-decoder`. Ao finalizar sua tarefa, ele adiciona os arquivos alterados ao staging e formaliza o checkpoint:
```bash
# Dentro de .worktrees/cpu-decoder
git add libs/cpu/ tests/unit/test_cpu.cpp

# Checkpoint explícito com preflight de gates e sem push
../../tools/agent-worktree.sh checkpoint -m "feat(cpu): add SSE2 opcode decoding" --gate-cmd "ctest --test-dir build -R test_cpu"
```

### Passo 5: Validação de Ownership e Integração Serial
Na raiz do repositório principal:
```bash
# 1. Validar ownership e ausência de alterações em hotspots
./tools/agent-worktree.sh validate-ownership cpu-decoder

# 2. Integrar serialmente na branch de integração
git checkout integration/m7-core-execution
git merge --no-ff agent/m7-core-execution/cpu-decoder -m "merge(cpu): integrate cpu-decoder from m7-core-execution"

# 3. Executar os gates da integração
ctest --test-dir build --output-on-failure
./tools/check-format.sh
openspec validate m7-core-execution --strict
```

### Passo 6: Remoção Segura do Worktree
Após integração confirmada e testes aprovados:
```bash
./tools/agent-worktree.sh remove cpu-decoder --delete-branch
```

---

## 6. Recuperação de Falhas e Retomada entre Sessões

Se uma sessão for interrompida (por reinicialização, timeout ou desconexão):
1. **Listar estado atual**:
   ```bash
   ./tools/agent-worktree.sh list
   ./tools/agent-worktree.sh status
   ```
2. **Inspecionar o manifesto**:
   Abra `.worktrees/manifest.json` para recuperar o `taskId`, a `lane` e o `baseSha`.
3. **Retomada estrita no mesmo path**:
   - NUNCA crie um novo worktree para uma tarefa em andamento.
   - Reative o agente apontando o `cwd` para o worktree existente (`.worktrees/<lane>`).
   - O agente inspeciona `git status` e `git log` dentro do worktree para continuar exatamente do ponto onde parou.

---

## 7. Troubleshooting e Diagnóstico

| Sintoma | Causa Provável | Ação de Resolução |
| :--- | :--- | :--- |
| `Cannot create worktree: working directory is dirty` | O repositório principal tem alterações não commitadas. | Revise, teste e comite (ou faça stash) as alterações do repositório principal antes de abrir lanes paralelas. |
| `Cannot remove worktree: uncommitted changes detected` | O worktree possui edições não salvas ou arquivos untracked. | Acesse o worktree, decida comitar ou descartar as edições deliberadamente. O script rejeita flags de force. |
| `Cannot remove worktree: branch has unmerged commits` | A branch da lane contém commits que ainda não foram integrados na base/main. | Faça a integração na main (`git merge`) ou verifique o log antes de prosseguir com a remoção. |
| `Invalid lane name` | Nome com caracteres proibidos (`..`, `/`, espaços). | Utilize apenas letras minúsculas/maiúsculas, dígitos, hífens e sublinhados (`[a-zA-Z0-9_-]`). |
