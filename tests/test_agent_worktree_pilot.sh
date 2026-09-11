#!/usr/bin/env bash
# tests/test_agent_worktree_pilot.sh
# Piloto completo de duas lanes independentes em repositório temporário sintético.
# Proibido executar checkpoints ou criar worktrees no repositório real.

set -euo pipefail

echo "================================================================="
echo "INICIANDO PILOTO EM REPOSITÓRIO TEMPORÁRIO SINTÉTICO"
echo "================================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKTREE_SCRIPT="${SCRIPT_DIR}/tools/agent-worktree.sh"

if [[ ! -f "$WORKTREE_SCRIPT" ]]; then
  echo "Erro: script $WORKTREE_SCRIPT não encontrado." >&2
  exit 1
fi

TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/xblob-pilot-XXXXXX")"
echo "Diretório temporário criado: ${TEMP_DIR}"

cleanup() {
  echo "Limpando diretório temporário: ${TEMP_DIR}..."
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# 1. Configurar repositório sintético
cd "$TEMP_DIR"
git init -b main
git config user.name "Pilot Agent"
git config user.email "agent@xblob.synthetic"

mkdir -p tools libs/c_api/include/xblob libs/cpu libs/gpu apps/desktop
cp "$WORKTREE_SCRIPT" tools/agent-worktree.sh
chmod +x tools/agent-worktree.sh

echo "cmake_minimum_required(VERSION 3.25)" > CMakeLists.txt
echo ".worktrees/" > .gitignore
echo "# AGENTS.md synthetic" > AGENTS.md
echo "# ARCHITECTURE.md synthetic" > ARCHITECTURE.md
echo "# README.md synthetic" > README.md
echo "// c_api.h" > libs/c_api/include/xblob/c_api.h
echo "// cpu.cpp" > libs/cpu/cpu.cpp
echo "// gpu.cpp" > libs/gpu/gpu.cpp
echo "{}" > apps/desktop/package-lock.json

git add .
git commit -m "chore: initial synthetic commit"
BASE_SHA="$(git rev-parse HEAD)"
echo "BASE_SHA estabelecido: ${BASE_SHA:0:8}"

# 2. Testar Checkpoint Seguro
echo ""
echo "--- TESTE 1: Checkpoint Explícito e Validações ---"

# 2.1 Checkpoint sem mensagem deve falhar
set +e
./tools/agent-worktree.sh checkpoint 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: checkpoint sem mensagem deveria ter sido recusado." >&2
  exit 1
fi
echo "✓ Checkpoint sem mensagem recusado corretamente."

# 2.2 Checkpoint com staging vazio deve falhar (auto-stage indiscriminado proibido)
set +e
./tools/agent-worktree.sh checkpoint -m "test: empty staging" 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: checkpoint com staging vazio deveria ter sido recusado." >&2
  exit 1
fi
echo "✓ Checkpoint com staging vazio recusado corretamente (sem auto-stage indiscriminado)."

# 2.3 Checkpoint com gate configurado que falha deve abortar
echo "// new cpu instruction" >> libs/cpu/cpu.cpp
git add libs/cpu/cpu.cpp
set +e
./tools/agent-worktree.sh checkpoint -m "feat(cpu): failed gate" --gate-cmd "exit 1" 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: checkpoint com gate falho deveria ter sido recusado." >&2
  exit 1
fi
echo "✓ Checkpoint bloqueado quando gate preflight falha."

# 2.4 Checkpoint válido com gate passando
./tools/agent-worktree.sh checkpoint -m "feat(cpu): add initial decode stub" --gate-cmd "test -f libs/cpu/cpu.cpp"
BASE_SHA="$(git rev-parse HEAD)"
echo "✓ Checkpoint explícito concluído com sucesso em ${BASE_SHA:0:8} (sem push)."

# 3. Testar init-integration
echo ""
echo "--- TESTE 2: init-integration Seguro ---"

# 3.1 Recusa se workspace principal estiver sujo
echo "uncommitted change" >> README.md
set +e
./tools/agent-worktree.sh init-integration marco-pilot 2>/dev/null
EXIT_CODE=$?
set -e
git checkout -- README.md
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: init-integration deveria ter sido recusado com workspace sujo." >&2
  exit 1
fi
echo "✓ init-integration recusado com workspace sujo."

# 3.2 init-integration com nome válido a partir de BASE_SHA
./tools/agent-worktree.sh init-integration marco-pilot "$BASE_SHA"
if ! git show-ref --verify --quiet refs/heads/integration/marco-pilot; then
  echo "Falha: branch refs/heads/integration/marco-pilot não encontrada." >&2
  exit 1
fi
echo "✓ Branch integration/marco-pilot criada a partir de ${BASE_SHA:0:8}."

# 3.3 init-integration recusa divergência existente
git checkout -b temp-diverge
echo "diverge" >> README.md
git commit -am "chore: diverge commit"
DIVERGE_SHA="$(git rev-parse HEAD)"
git update-ref refs/heads/integration/marco-pilot "$DIVERGE_SHA"
git checkout main
git branch -D temp-diverge

set +e
./tools/agent-worktree.sh init-integration marco-pilot "$BASE_SHA" 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: init-integration deveria ter recusado branch divergente." >&2
  exit 1
fi
echo "✓ init-integration recusou divergência existente com segurança."

# Restaurar integration/marco-pilot para BASE_SHA
git update-ref refs/heads/integration/marco-pilot "$BASE_SHA"

# 4. Criar duas lanes independentes com DAG e ownership
echo ""
echo "--- TESTE 3: Criação de Duas Lanes Independentes com DAG ---"

./tools/agent-worktree.sh create cpu-lane "$BASE_SHA" \
  --parent marco-pilot \
  --change "feat-cpu-core" \
  --owned "libs/cpu/**" \
  --denied "CMakeLists.txt,libs/c_api/**,libs/machine/**"

./tools/agent-worktree.sh create gpu-lane "$BASE_SHA" \
  --parent marco-pilot \
  --change "feat-gpu-pipeline" \
  --owned "libs/gpu/**" \
  --denied "CMakeLists.txt,libs/c_api/**,libs/machine/**"

./tools/agent-worktree.sh set-task cpu-lane "task-cpu-001"
./tools/agent-worktree.sh set-task gpu-lane "task-gpu-002"

echo "=== Listagem e Manifesto com Schema 2.0 ==="
./tools/agent-worktree.sh list
./tools/agent-worktree.sh list --json

# 5. Testar validate-ownership e bloqueio de violação
echo ""
echo "--- TESTE 4: Validação de Ownership e Detecção de Violações ---"

# 5.1 Edição válida na lane CPU
(
  cd .worktrees/cpu-lane
  echo "void decode_x86() {}" >> libs/cpu/cpu.cpp
  git add libs/cpu/cpu.cpp
  ../../tools/agent-worktree.sh checkpoint -m "feat(cpu): implement decode_x86" --gate-cmd "test -f libs/cpu/cpu.cpp"
)

# Validar ownership: deve ser APROVADO
./tools/agent-worktree.sh validate-ownership cpu-lane
echo "✓ Ownership da cpu-lane validado e aprovado para arquivos exclusivos."

# 5.2 Violação de hotspot na lane CPU (edição indevida de CMakeLists.txt)
(
  cd .worktrees/cpu-lane
  echo "# hotspot unauthorized edit" >> CMakeLists.txt
)

set +e
./tools/agent-worktree.sh validate-ownership cpu-lane 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: validate-ownership deveria ter REJEITADO alteração em hotspot protegido (CMakeLists.txt)." >&2
  exit 1
fi
echo "✓ validate-ownership bloqueou com sucesso violação de hotspot em CMakeLists.txt."

# Reverter violação não autorizada
(
  cd .worktrees/cpu-lane
  git checkout -- CMakeLists.txt
)
./tools/agent-worktree.sh validate-ownership cpu-lane
echo "✓ Após reversão, validate-ownership aprovado novamente."

# 5.3 Edição válida na lane GPU
(
  cd .worktrees/gpu-lane
  echo "void render_nv2a() {}" >> libs/gpu/gpu.cpp
  git add libs/gpu/gpu.cpp
  ../../tools/agent-worktree.sh checkpoint -m "feat(gpu): implement render_nv2a" --gate-cmd "test -f libs/gpu/gpu.cpp"
)

./tools/agent-worktree.sh validate-ownership gpu-lane
echo "✓ Ownership da gpu-lane validado e aprovado."

# 6. Integração serial na branch integration/marco-pilot
echo ""
echo "--- TESTE 5: Integração Serial com Gates Simulados ---"

git checkout integration/marco-pilot

# Merge serial da Lane 1
echo "Integrando serialmente cpu-lane..."
git merge --no-ff agent/marco-pilot/cpu-lane -m "merge(cpu): integrate cpu-lane into marco-pilot"
# Executar gate simulado
test -f libs/cpu/cpu.cpp
echo "✓ Gate simulado pós-integração de cpu-lane aprovado."

# Merge serial da Lane 2
echo "Integrando serialmente gpu-lane..."
git merge --no-ff agent/marco-pilot/gpu-lane -m "merge(gpu): integrate gpu-lane into marco-pilot"
# Executar gate simulado
test -f libs/gpu/gpu.cpp
echo "✓ Gate simulado pós-integração de gpu-lane aprovado."

# Integrar a branch de integração na main
git checkout main
git merge --no-ff integration/marco-pilot -m "merge(integration): integrate marco-pilot into main"
echo "✓ Branch integration/marco-pilot integrada na main com sucesso."

# 7. Remoção Segura e Prevenção de Perda de Dados
echo ""
echo "--- TESTE 6: Remoção Segura e Regras sem Force ---"

# 7.1 Recusa remoção se worktree estiver com alterações (dirty)
echo "uncommitted temp work" >> .worktrees/cpu-lane/dirty.tmp
set +e
./tools/agent-worktree.sh remove cpu-lane 2>/dev/null
EXIT_CODE=$?
set -e
rm -f .worktrees/cpu-lane/dirty.tmp
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: remove deveria recusar worktree sujo." >&2
  exit 1
fi
echo "✓ Remoção de worktree sujo bloqueada sem perda de dados."

# 7.2 Recusa remoção se branch possuir commits não integrados
./tools/agent-worktree.sh create unmerged-lane "$BASE_SHA" --parent marco-pilot
(
  cd .worktrees/unmerged-lane
  echo "unmerged work" >> unmerged.txt
  git add unmerged.txt
  git commit -m "feat: unmerged commit"
)
set +e
./tools/agent-worktree.sh remove unmerged-lane 2>/dev/null
EXIT_CODE=$?
set -e
if [[ $EXIT_CODE -eq 0 ]]; then
  echo "Falha: remove deveria recusar branch com commits não integrados." >&2
  exit 1
fi
echo "✓ Remoção bloqueada para branch com commits não integrados."

# Limpar unmerged-lane integrando-a ou resetando
(
  cd .worktrees/unmerged-lane
  git reset --hard "$BASE_SHA"
)
./tools/agent-worktree.sh remove unmerged-lane --delete-branch

# 7.3 Remover lanes integradas com segurança
./tools/agent-worktree.sh remove cpu-lane --delete-branch
./tools/agent-worktree.sh remove gpu-lane --delete-branch

echo ""
echo "=== Estado Final do Manifesto ==="
./tools/agent-worktree.sh list

echo ""
echo "================================================================="
echo "PILOTO DE DUAS LANES CONCLUÍDO COM SUCESSO TOTAL!"
echo "================================================================="
