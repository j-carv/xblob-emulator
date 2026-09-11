#!/usr/bin/env bash
# agent-worktree.sh - Gerenciamento seguro de worktrees Git para agentes paralelos (ACP)
# Proibição estrita de force (-f/--force) e deleções destrutivas não integradas.

set -euo pipefail

show_help() {
  cat << 'EOF'
Uso:
  agent-worktree.sh create <lane> [base-commit] [--parent <parent-change>] [--change <change>] [--owned <globs>] [--denied <globs>] [--depends-on <lanes>]
  agent-worktree.sh checkpoint -m "<mensagem>" [--change <change>] [--gate-cmd "<cmd>"]
  agent-worktree.sh init-integration <parent-change> [base-commit]
  agent-worktree.sh validate-ownership <lane> [--base <base-commit>] [--json]
  agent-worktree.sh list [--json]
  agent-worktree.sh status [lane]
  agent-worktree.sh set-task <lane> <task-id>
  agent-worktree.sh set-lane-meta <lane> [--change <change>] [--owned <globs>] [--denied <globs>] [--depends-on <lanes>]
  agent-worktree.sh remove <lane> [--delete-branch]
  agent-worktree.sh help

Subcomandos:
  create              Cria um worktree isolado em .worktrees/<lane> para a lane informada.
                      Requer obrigatoriamente que o repositório principal esteja limpo.
  checkpoint          Executa commit explícito de staging com preflight de gates e OpenSpec.
                      Exige mensagem (-m), recusa staging vazio e NUNCA faz push automático.
  init-integration    Inicializa a branch de integração 'integration/<parent>' a partir do BASE_SHA.
                      Recusa dirty state, nomes inválidos e divergência existente.
  validate-ownership  Compara diff da lane contra BASE_SHA, bloqueia arquivos fora de ownership
                      ou em hotspots protegidos e produz relatório estável.
  list                Lista os worktrees gerenciados e status resumido do manifesto.
  status              Exibe detalhes da lane, cleanliness e integração de commits.
  set-task            Registra o ACP task ID da lane no manifesto local.
  set-lane-meta       Atualiza metadados (change, owned, denied, dependencies) da lane.
  remove              Remove com segurança um worktree limpo e integrado (sem force).
  help                Exibe esta mensagem de ajuda.

Regras de Segurança:
  - Nenhuma operação utiliza --force por padrão.
  - Recusa criação sobre workspace sujo (uncommitted changes).
  - Recusa remoção de worktree com alterações locais (dirty).
  - Recusa remoção se a branch contiver commits não integrados.
  - Nomes de lane e parent são validados contra path traversal e injeção (regex ^[a-zA-Z0-9_-]+$).
  - Hotspots protegidos (CMake raiz, C ABI, machine, lockfiles, docs globais) são reservados à integração.
EOF
}

get_repo_root() {
  git rev-parse --show-toplevel 2>/dev/null || {
    echo "Erro: não foi possível localizar a raiz do repositório Git." >&2
    exit 1
  }
}

validate_identifier() {
  local id="$1"
  local field_name="$2"
  if [[ ! "$id" =~ ^[a-zA-Z0-9_-]+$ ]]; then
    echo "Erro: identificador inválido para ${field_name}: '${id}'. Use apenas [a-zA-Z0-9_-]." >&2
    exit 1
  fi
}

manifest_path() {
  local root="$1"
  echo "${root}/.worktrees/manifest.json"
}

init_manifest_if_needed() {
  local root="$1"
  local mfile
  mfile="$(manifest_path "$root")"
  if [[ ! -f "$mfile" ]]; then
    mkdir -p "$(dirname "$mfile")"
    cat << 'EOF' > "$mfile"
{
  "schemaVersion": "2.0",
  "version": "2.0",
  "parent": "",
  "integrationBranch": "",
  "baseSha": "",
  "worktrees": []
}
EOF
  else
    # Migração / retrocompatibilidade automática para schemaVersion 2.0
    if command -v python3 >/dev/null 2>&1; then
      python3 - << PYEOF
import json

mpath = "$mfile"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception:
    data = {}

changed = False
if data.get("schemaVersion") != "2.0" or data.get("version") != "2.0":
    data["schemaVersion"] = "2.0"
    data["version"] = "2.0"
    changed = True

if "parent" not in data:
    data["parent"] = ""
    changed = True
if "integrationBranch" not in data:
    data["integrationBranch"] = ""
    changed = True
if "baseSha" not in data:
    data["baseSha"] = ""
    changed = True
if "worktrees" not in data or not isinstance(data["worktrees"], list):
    data["worktrees"] = []
    changed = True

for w in data["worktrees"]:
    if "change" not in w:
        w["change"] = ""
        changed = True
    if "owned" not in w or not isinstance(w["owned"], list):
        w["owned"] = []
        changed = True
    if "denied" not in w or not isinstance(w["denied"], list):
        w["denied"] = []
        changed = True
    if "dependencies" not in w or not isinstance(w["dependencies"], list):
        w["dependencies"] = []
        changed = True

if changed:
    with open(mpath, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
PYEOF
    fi
  fi
}

cmd_checkpoint() {
  local root
  root="$(get_repo_root)"

  local message=""
  local change=""
  local gate_cmd=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      -m|--message)
        message="${2:-}"
        if [[ -z "$message" ]]; then
          echo "Erro: opção -m/--message requer uma mensagem não vazia." >&2
          exit 1
        fi
        shift 2
        ;;
      --change)
        change="${2:-}"
        if [[ -z "$change" ]]; then
          echo "Erro: opção --change requer um nome de change." >&2
          exit 1
        fi
        shift 2
        ;;
      --gate-cmd)
        gate_cmd="${2:-}"
        if [[ -z "$gate_cmd" ]]; then
          echo "Erro: opção --gate-cmd requer um comando." >&2
          exit 1
        fi
        shift 2
        ;;
      *)
        echo "Erro: argumento inesperado '$1' para checkpoint." >&2
        exit 1
        ;;
    esac
  done

  if [[ -z "$message" ]]; then
    echo "Erro: mensagem de commit obrigatória para o checkpoint. Use -m \"mensagem\"." >&2
    exit 1
  fi

  # Validação 1: Staging não pode estar vazio (auto-stage indiscriminado é proibido)
  if git -C "$root" diff --cached --quiet 2>/dev/null; then
    echo "Erro: nenhuma alteração em staging para commit." >&2
    echo "Adicione os arquivos desejados com 'git add' antes de invocar checkpoint (auto-staging indiscriminado é proibido por governança)." >&2
    exit 1
  fi

  echo "=== Revisão de Arquivos em Staging ==="
  git -C "$root" diff --cached --name-status

  # Validação 2: Preflight OpenSpec
  if [[ -n "$change" ]]; then
    if command -v openspec >/dev/null 2>&1; then
      echo "Executando preflight OpenSpec strict para '${change}'..."
      if ! (cd "$root" && openspec validate "$change" --strict); then
        echo "Erro: preflight OpenSpec strict falhou para a change '${change}'. Checkpoint abortado." >&2
        exit 1
      fi
    fi
  else
    # Detectar se há changes em staging
    local staged_changes
    staged_changes="$(git -C "$root" diff --cached --name-only | grep -E '^openspec/changes/[^/]+/' | cut -d/ -f3 | sort -u || true)"
    if [[ -n "$staged_changes" ]] && command -v openspec >/dev/null 2>&1; then
      for c in $staged_changes; do
        echo "Executando preflight OpenSpec strict para '${c}' detectada em staging..."
        if ! (cd "$root" && openspec validate "$c" --strict); then
          echo "Erro: preflight OpenSpec strict falhou para '${c}'. Checkpoint abortado." >&2
          exit 1
        fi
      done
    fi
  fi

  # Validação 3: Preflight de Gates
  if [[ -n "$gate_cmd" ]]; then
    echo "Executando gate preflight configurado: ${gate_cmd}..."
    if ! (cd "$root" && eval "$gate_cmd"); then
      echo "Erro: o gate de verificação preflight falhou. Checkpoint abortado." >&2
      exit 1
    fi
  else
    if [[ -x "${root}/tools/check-format.sh" ]]; then
      echo "Executando verificação de formatação (${root}/tools/check-format.sh)..."
      if ! (cd "$root" && "${root}/tools/check-format.sh"); then
        echo "Erro: verificação de formatação falhou. Checkpoint abortado." >&2
        exit 1
      fi
    fi
  fi

  echo "Criando commit de checkpoint..."
  git -C "$root" commit -m "$message"
  local new_sha
  new_sha="$(git -C "$root" rev-parse --short HEAD)"
  echo "Checkpoint criado com sucesso no commit ${new_sha}. (Push automático desabilitado por governança)."
}

cmd_init_integration() {
  local root
  root="$(get_repo_root)"

  if [[ $# -lt 1 ]]; then
    echo "Erro: informe o identificador da parent change. Exemplo: agent-worktree.sh init-integration marco-7" >&2
    exit 1
  fi

  local parent="$1"
  shift
  validate_identifier "$parent" "parent"

  local base_ref="${1:-HEAD}"

  # Validação 1: Workspace principal DEVE estar rigorosamente limpo
  local dirty_status
  dirty_status="$(git -C "$root" status --porcelain 2>/dev/null || true)"
  if [[ -n "$dirty_status" ]]; then
    echo "Erro: o workspace principal possui alterações não commitadas (dirty)." >&2
    echo "A inicialização da branch de integração requer que a base esteja limpa e validada." >&2
    exit 1
  fi

  # Validação 2: Base commit válida
  local base_sha
  base_sha="$(git -C "$root" rev-parse --verify "${base_ref}^{commit}" 2>/dev/null || true)"
  if [[ -z "$base_sha" ]]; then
    echo "Erro: commit base inválido '${base_ref}'." >&2
    exit 1
  fi

  local branch="integration/${parent}"

  # Validação 3: Se a branch já existe, verificar se diverge do BASE_SHA
  if git -C "$root" show-ref --verify --quiet "refs/heads/${branch}" 2>/dev/null; then
    local existing_sha
    existing_sha="$(git -C "$root" rev-parse "refs/heads/${branch}")"
    if [[ "$existing_sha" != "$base_sha" ]]; then
      echo "Erro: a branch '${branch}' já existe em '${existing_sha:0:8}' e diverge do BASE_SHA informado '${base_sha:0:8}'." >&2
      exit 1
    else
      echo "A branch '${branch}' já existe e coincide com o commit base (${base_sha:0:8})."
    fi
  else
    git -C "$root" branch "$branch" "$base_sha"
    echo "Branch '${branch}' criada com sucesso a partir de ${base_sha:0:8}."
  fi

  # Atualizar manifesto
  init_manifest_if_needed "$root"
  if command -v python3 >/dev/null 2>&1; then
    python3 - << PYEOF
import json

mpath = "$(manifest_path "$root")"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception:
    data = {"schemaVersion": "2.0", "version": "2.0", "worktrees": []}

data["schemaVersion"] = "2.0"
data["version"] = "2.0"
data["parent"] = "$parent"
data["integrationBranch"] = "$branch"
data["baseSha"] = "$base_sha"

with open(mpath, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)
PYEOF
  fi

  echo "Integração inicializada para parent '${parent}' na branch '${branch}'."
}

cmd_create() {
  local root
  root="$(get_repo_root)"

  if [[ $# -lt 1 ]]; then
    echo "Erro: informe o nome da lane. Exemplo: agent-worktree.sh create cpu" >&2
    exit 1
  fi

  local lane="$1"
  shift
  validate_identifier "$lane" "lane"

  local base_ref="HEAD"
  local parent=""
  local change=""
  local owned=""
  local denied=""
  local depends_on=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --parent)
        parent="${2:-}"
        if [[ -z "$parent" ]]; then
          echo "Erro: opção --parent requer um argumento." >&2
          exit 1
        fi
        shift 2
        ;;
      --change)
        change="${2:-}"
        if [[ -z "$change" ]]; then
          echo "Erro: opção --change requer um argumento." >&2
          exit 1
        fi
        shift 2
        ;;
      --owned)
        owned="${2:-}"
        shift 2
        ;;
      --denied)
        denied="${2:-}"
        shift 2
        ;;
      --depends-on)
        depends_on="${2:-}"
        shift 2
        ;;
      *)
        if [[ "$base_ref" == "HEAD" ]]; then
          base_ref="$1"
        else
          echo "Erro: argumento inesperado '$1'." >&2
          exit 1
        fi
        shift
        ;;
    esac
  done

  # Se parent não foi passado, verificar se há parent no manifesto
  init_manifest_if_needed "$root"
  if [[ -z "$parent" ]] && command -v python3 >/dev/null 2>&1; then
    local m_parent
    m_parent="$(python3 -c "
import json
try:
    with open('$(manifest_path "$root")') as f:
        print(json.load(f).get('parent', ''))
except: pass
")"
    if [[ -n "$m_parent" ]]; then
      parent="$m_parent"
    fi
  fi

  if [[ -n "$parent" ]]; then
    validate_identifier "$parent" "parent"
  fi

  # Validação 1: Workspace principal DEVE estar rigorosamente limpo
  local dirty_status
  dirty_status="$(git -C "$root" status --porcelain 2>/dev/null || true)"
  if [[ -n "$dirty_status" ]]; then
    echo "Erro: o workspace principal possui alterações não commitadas (dirty)." >&2
    echo "A criação de worktrees paralelos requer que a base esteja limpa e validada." >&2
    echo "Efetue commit ou stash das alterações antes de criar lanes." >&2
    exit 1
  fi

  # Validação 2: Base commit válida
  local base_sha
  base_sha="$(git -C "$root" rev-parse --verify "${base_ref}^{commit}" 2>/dev/null || true)"
  if [[ -z "$base_sha" ]]; then
    echo "Erro: commit base inválido '${base_ref}'." >&2
    exit 1
  fi

  local wt_rel_dir=".worktrees/${lane}"
  local wt_abs_dir="${root}/${wt_rel_dir}"

  # Validação 3: Path escape e existência
  local canonical_base canonical_target
  canonical_base="$(cd "${root}" && pwd -P)/.worktrees"
  canonical_target="$(cd "${root}" 2>/dev/null && python3 -c "import os, sys; print(os.path.normpath(os.path.join(sys.argv[1], sys.argv[2])))" "$canonical_base" "$lane")"
  if [[ "$canonical_target" != "${canonical_base}/${lane}" ]]; then
    echo "Erro de segurança: tentativa de path traversal ou symlink escape detectada para '${lane}'." >&2
    exit 1
  fi

  if [[ -e "$wt_abs_dir" ]]; then
    echo "Erro: diretório de worktree já existe em '${wt_rel_dir}'." >&2
    exit 1
  fi

  local branch
  if [[ -n "$parent" ]]; then
    branch="agent/${parent}/${lane}"
  else
    branch="agent/${lane}"
  fi

  # Validação 4: Branch já existe?
  if git -C "$root" show-ref --verify --quiet "refs/heads/${branch}" 2>/dev/null; then
    echo "Erro: a branch '${branch}' já existe." >&2
    exit 1
  fi

  mkdir -p "${root}/.worktrees"

  echo "Criando worktree '${lane}' em '${wt_rel_dir}' na branch '${branch}' (base ${base_sha:0:8})..."
  # Sem uso de --force
  git -C "$root" worktree add -b "$branch" "$wt_abs_dir" "$base_sha"

  # Registrar no manifesto
  local now
  now="$(date -u +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null || date +"%Y-%m-%dT%H:%M:%SZ")"

  if command -v python3 >/dev/null 2>&1; then
    python3 - << PYEOF
import json, sys

mpath = "$(manifest_path "$root")"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception:
    data = {"schemaVersion": "2.0", "version": "2.0", "worktrees": []}

data["schemaVersion"] = "2.0"
data["version"] = "2.0"

owned_str = "$owned"
denied_str = "$denied"
deps_str = "$depends_on"

owned_list = [x.strip() for x in owned_str.split(",") if x.strip()]
denied_list = [x.strip() for x in denied_str.split(",") if x.strip()]
deps_list = [x.strip() for x in deps_str.split(",") if x.strip()]

worktrees = [w for w in data.get("worktrees", []) if w.get("lane") != "$lane"]
worktrees.append({
    "lane": "$lane",
    "parent": "$parent",
    "branch": "$branch",
    "path": "$wt_rel_dir",
    "baseSha": "$base_sha",
    "createdAt": "$now",
    "taskId": "",
    "change": "$change",
    "owned": owned_list,
    "denied": denied_list,
    "dependencies": deps_list
})
data["worktrees"] = worktrees

with open(mpath, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)
PYEOF
  fi

  echo "Worktree criado com sucesso em '${wt_rel_dir}'."
}

cmd_set_task() {
  local root
  root="$(get_repo_root)"
  if [[ $# -lt 2 ]]; then
    echo "Erro: informe a lane e o task-id. Exemplo: agent-worktree.sh set-task cpu task-1234" >&2
    exit 1
  fi
  local lane="$1"
  local task_id="$2"
  validate_identifier "$lane" "lane"

  init_manifest_if_needed "$root"
  if command -v python3 >/dev/null 2>&1; then
    python3 - << PYEOF
import json, sys
mpath = "$(manifest_path "$root")"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception:
    data = {"schemaVersion": "2.0", "version": "2.0", "worktrees": []}

found = False
for w in data.get("worktrees", []):
    if w.get("lane") == "$lane":
        w["taskId"] = "$task_id"
        found = True
        break

if not found:
    data.setdefault("worktrees", []).append({
        "lane": "$lane",
        "parent": "",
        "branch": "",
        "path": ".worktrees/$lane",
        "baseSha": "",
        "createdAt": "",
        "taskId": "$task_id",
        "change": "",
        "owned": [],
        "denied": [],
        "dependencies": []
    })

with open(mpath, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)
print("Task ID '$task_id' registrado para lane '$lane'.")
PYEOF
  fi
}

cmd_set_lane_meta() {
  local root
  root="$(get_repo_root)"
  if [[ $# -lt 1 ]]; then
    echo "Erro: informe a lane. Exemplo: agent-worktree.sh set-lane-meta cpu --change <change>" >&2
    exit 1
  fi

  local lane="$1"
  shift
  validate_identifier "$lane" "lane"

  local change=""
  local change_set=false
  local owned=""
  local owned_set=false
  local denied=""
  local denied_set=false
  local depends_on=""
  local deps_set=false

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --change)
        change="${2:-}"
        change_set=true
        shift 2
        ;;
      --owned)
        owned="${2:-}"
        owned_set=true
        shift 2
        ;;
      --denied)
        denied="${2:-}"
        denied_set=true
        shift 2
        ;;
      --depends-on)
        depends_on="${2:-}"
        deps_set=true
        shift 2
        ;;
      *)
        echo "Erro: argumento inesperado '$1'." >&2
        exit 1
        ;;
    esac
  done

  init_manifest_if_needed "$root"
  if command -v python3 >/dev/null 2>&1; then
    python3 - << PYEOF
import json, sys

mpath = "$(manifest_path "$root")"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception:
    data = {"schemaVersion": "2.0", "version": "2.0", "worktrees": []}

found = False
for w in data.get("worktrees", []):
    if w.get("lane") == "$lane":
        found = True
        if "$change_set" == "true":
            w["change"] = "$change"
        if "$owned_set" == "true":
            w["owned"] = [x.strip() for x in "$owned".split(",") if x.strip()]
        if "$denied_set" == "true":
            w["denied"] = [x.strip() for x in "$denied".split(",") if x.strip()]
        if "$deps_set" == "true":
            w["dependencies"] = [x.strip() for x in "$depends_on".split(",") if x.strip()]
        break

if not found:
    print("Erro: lane '$lane' não encontrada no manifesto.", file=sys.stderr)
    sys.exit(1)

with open(mpath, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)
print("Metadados atualizados com sucesso para lane '$lane'.")
PYEOF
  fi
}

cmd_validate_ownership() {
  local root
  root="$(get_repo_root)"

  if [[ $# -lt 1 ]]; then
    echo "Erro: informe o nome da lane. Exemplo: agent-worktree.sh validate-ownership cpu" >&2
    exit 1
  fi

  local lane="$1"
  shift
  validate_identifier "$lane" "lane"

  local base_override=""
  local output_json=false

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --base)
        base_override="${2:-}"
        if [[ -z "$base_override" ]]; then
          echo "Erro: opção --base requer um commit." >&2
          exit 1
        fi
        shift 2
        ;;
      --json)
        output_json=true
        shift
        ;;
      *)
        echo "Erro: argumento inesperado '$1' para validate-ownership." >&2
        exit 1
        ;;
    esac
  done

  init_manifest_if_needed "$root"

  # Executa verificação determinística via Python
  python3 - << PYEOF
import json, subprocess, sys, fnmatch, os

root = "$root"
lane = "$lane"
base_override = "$base_override"
output_json = ("$output_json" == "true")

mpath = os.path.join(root, ".worktrees", "manifest.json")
lane_data = None
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        mdata = json.load(f)
        for w in mdata.get("worktrees", []):
            if w.get("lane") == lane:
                lane_data = w
                break
except Exception as e:
    mdata = {}

branch = lane_data.get("branch", f"agent/{lane}") if lane_data else f"agent/{lane}"
base_sha = base_override or (lane_data.get("baseSha") if lane_data else "")

if not base_sha:
    try:
        base_sha = subprocess.check_output(["git", "-C", root, "merge-base", branch, "HEAD"], stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        base_sha = "HEAD"

# Hotspots protegidos globais (AGENTS.md 9.2)
DEFAULT_HOTSPOTS = [
    "CMakeLists.txt",
    "libs/c_api/**",
    "libs/machine/**",
    "apps/desktop/package-lock.json",
    "Cargo.lock",
    "AGENTS.md",
    "README.md",
    "ARCHITECTURE.md"
]

owned_patterns = lane_data.get("owned", []) if lane_data else []
denied_patterns = DEFAULT_HOTSPOTS + (lane_data.get("denied", []) if lane_data else [])

def normalize_path(p):
    return p.strip().replace("\\\\", "/")

def match_glob(path, pat):
    path = normalize_path(path)
    pat = normalize_path(pat)
    if pat.endswith("/**"):
        prefix = pat[:-3].rstrip("/")
        if path == prefix or path.startswith(prefix + "/"):
            return True
    return fnmatch.fnmatch(path, pat)

# Obter diff de arquivos alterados entre baseSha e branch
diff_files = set()
try:
    out = subprocess.check_output(["git", "-C", root, "diff", "--name-only", f"{base_sha}..{branch}"], stderr=subprocess.DEVNULL, text=True)
    for line in out.splitlines():
        if line.strip():
            diff_files.add(normalize_path(line))
except Exception as e:
    pass

# Verificar se há arquivos alterados/untracked dentro do worktree físico da lane se existir
wt_path = os.path.join(root, ".worktrees", lane)
if os.path.isdir(wt_path):
    try:
        out = subprocess.check_output(["git", "-C", wt_path, "status", "--porcelain"], stderr=subprocess.DEVNULL, text=True)
        for line in out.splitlines():
            if len(line) >= 3:
                fpath = line[3:].strip()
                if " -> " in fpath:
                    fpath = fpath.split(" -> ")[1]
                diff_files.add(normalize_path(fpath))
    except Exception:
        pass

changed_list = sorted(list(diff_files))
violations = []

for f in changed_list:
    # 1. Checar se coincide com padrão negado/hotspot
    is_denied = False
    for pat in denied_patterns:
        if match_glob(f, pat):
            violations.append({
                "file": f,
                "reason": "denied_hotspot",
                "pattern": pat
            })
            is_denied = True
            break
    if is_denied:
        continue

    # 2. Se houver lista de owned patterns, arquivo deve bater com pelo menos um
    if owned_patterns:
        is_owned = any(match_glob(f, pat) for pat in owned_patterns)
        if not is_owned:
            violations.append({
                "file": f,
                "reason": "outside_ownership",
                "pattern": ""
            })

is_valid = (len(violations) == 0)

if output_json:
    report = {
        "lane": lane,
        "branch": branch,
        "baseSha": base_sha,
        "valid": is_valid,
        "totalChangedFiles": len(changed_list),
        "changedFiles": changed_list,
        "ownedPatterns": owned_patterns,
        "deniedPatterns": denied_patterns,
        "violations": violations
    }
    print(json.dumps(report, indent=2))
else:
    print(f"=== Relatório de Ownership da Lane '{lane}' ===")
    print(f"Branch: {branch}")
    print(f"Base SHA: {base_sha[:8] if len(base_sha) >= 8 else base_sha}")
    print(f"Total de arquivos modificados: {len(changed_list)}")
    if changed_list:
        print("Arquivos detectados:")
        for f in changed_list:
            print(f"  - {f}")
    else:
        print("  (Nenhum arquivo alterado em relação à base)")

    if violations:
        print("\nVIOLAÇÕES DE OWNERSHIP / HOTSPOTS ENCONTRADAS:")
        for v in violations:
            if v["reason"] == "denied_hotspot":
                print(f"  [BLOQUEADO - HOTSPOT/DENIED] '{v['file']}' (coincide com padrão '{v['pattern']}')")
            else:
                print(f"  [BLOQUEADO - FORA DE ESCOPO] '{v['file']}' (fora dos padrões owned declarados)")
        print("\nResultado: REJEITADO (violação do protocolo de ownership disjunto)")
    else:
        print("\nResultado: APROVADO (nenhuma violação de ownership ou hotspot)")

if not is_valid:
    sys.exit(1)
PYEOF
}

cmd_list() {
  local root
  root="$(get_repo_root)"

  local output_json=false
  if [[ "${1:-}" == "--json" ]]; then
    output_json=true
  fi

  local mfile
  mfile="$(manifest_path "$root")"
  init_manifest_if_needed "$root"

  if [[ "$output_json" == true ]]; then
    if command -v python3 >/dev/null 2>&1; then
      python3 -c "import json; print(json.dumps(json.load(open('$mfile')), indent=2))"
    else
      cat "$mfile"
    fi
    return
  fi

  echo "=== Worktrees Git Registrados ==="
  git -C "$root" worktree list

  if [[ -f "$mfile" ]]; then
    echo ""
    echo "=== Manifesto Local (.worktrees/manifest.json) ==="
    if command -v python3 >/dev/null 2>&1; then
      python3 - << PYEOF
import json
try:
    with open("$mfile", 'r', encoding='utf-8') as f:
        data = json.load(f)
    print(f"SchemaVersion: {data.get('schemaVersion', '2.0')} | Parent: {data.get('parent') or '(nenhum)'} | Integration: {data.get('integrationBranch') or '(nenhuma)'}")
    wts = data.get("worktrees", [])
    if not wts:
        print("Nenhuma lane registrada no manifesto.")
    for w in wts:
        deps = ",".join(w.get('dependencies', [])) or "none"
        owned = ",".join(w.get('owned', [])) or "all"
        tid = w.get('taskId', '') or '(nenhum)'
        bsha = (w.get('baseSha', '') or '')[:8]
        print(f"- Lane: {w.get('lane')} | Branch: {w.get('branch')} | Base: {bsha} | TaskId: {tid} | Deps: [{deps}] | Owned: [{owned}]")
except Exception as e:
    print(f"Erro ao ler manifesto: {e}")
PYEOF
    else
      cat "$mfile"
    fi
  fi
}

cmd_status() {
  local root
  root="$(get_repo_root)"
  local target_lane="${1:-}"

  if [[ -n "$target_lane" ]]; then
    validate_identifier "$target_lane" "lane"
    local wt_rel_dir=".worktrees/${target_lane}"
    local wt_abs_dir="${root}/${wt_rel_dir}"

    if [[ ! -d "$wt_abs_dir" ]]; then
      echo "Erro: worktree para lane '${target_lane}' não encontrado em '${wt_rel_dir}'." >&2
      exit 1
    fi

    echo "=== Status da Lane '${target_lane}' ==="
    echo "Diretório: ${wt_rel_dir}"
    local cur_branch
    cur_branch="$(git -C "$wt_abs_dir" branch --show-current 2>/dev/null || echo "(desconhecida)")"
    echo "Branch: ${cur_branch}"
    local cur_commit
    cur_commit="$(git -C "$wt_abs_dir" rev-parse --short HEAD 2>/dev/null || echo "(desconhecido)")"
    echo "HEAD: ${cur_commit}"

    local wt_dirty
    wt_dirty="$(git -C "$wt_abs_dir" status --porcelain 2>/dev/null || true)"
    if [[ -n "$wt_dirty" ]]; then
      echo "Estado: MODIFICADO / DIRTY"
      echo "$wt_dirty"
    else
      echo "Estado: LIMPO"
    fi

    # Ler dados do manifesto
    local mfile
    mfile="$(manifest_path "$root")"
    if [[ -f "$mfile" ]] && command -v python3 >/dev/null 2>&1; then
      python3 - << PYEOF
import json
try:
    with open("$mfile") as f:
        data = json.load(f)
    for w in data.get("worktrees", []):
        if w.get("lane") == "$target_lane":
            print(f"Parent: {w.get('parent') or '(não definido)'}")
            print(f"Change: {w.get('change') or '(não definido)'}")
            print(f"Task ID: {w.get('taskId') or '(não definido)'}")
            print(f"Owned globs: {w.get('owned')}")
            print(f"Denied globs: {w.get('denied')}")
            print(f"Dependencies: {w.get('dependencies')}")
            break
except Exception:
    pass
PYEOF
    fi

    # Verificar integração com HEAD e com branch de integração
    local main_head
    main_head="$(git -C "$root" rev-parse HEAD 2>/dev/null || true)"
    if [[ -n "$cur_branch" && -n "$main_head" ]]; then
      if git -C "$root" merge-base --is-ancestor "$cur_branch" "$main_head" 2>/dev/null; then
        echo "Integração (HEAD principal): INTEGRADO (${main_head:0:8})"
      else
        local unmerged_count
        unmerged_count="$(git -C "$root" rev-list --count "${main_head}..${cur_branch}" 2>/dev/null || echo "?")"
        echo "Integração (HEAD principal): PENDENTE (${unmerged_count} commit(s) à frente)"
      fi
    fi

    # Checar branch de integração se existir
    local int_branch=""
    if [[ -f "$mfile" ]] && command -v python3 >/dev/null 2>&1; then
      int_branch="$(python3 -c "
import json
try:
    with open('$mfile') as f:
        print(json.load(f).get('integrationBranch', ''))
except: pass
")"
    fi

    if [[ -n "$int_branch" ]] && git -C "$root" show-ref --verify --quiet "refs/heads/${int_branch}" 2>/dev/null; then
      local int_head
      int_head="$(git -C "$root" rev-parse "refs/heads/${int_branch}" 2>/dev/null || true)"
      if git -C "$root" merge-base --is-ancestor "$cur_branch" "$int_head" 2>/dev/null; then
        echo "Integração (${int_branch}): INTEGRADO (${int_head:0:8})"
      else
        local unmerged_int
        unmerged_int="$(git -C "$root" rev-list --count "${int_head}..${cur_branch}" 2>/dev/null || echo "?")"
        echo "Integração (${int_branch}): PENDENTE (${unmerged_int} commit(s) à frente)"
      fi
    fi
  else
    cmd_list
  fi
}

cmd_remove() {
  local root
  root="$(get_repo_root)"

  if [[ $# -lt 1 ]]; then
    echo "Erro: informe a lane a remover. Exemplo: agent-worktree.sh remove cpu" >&2
    exit 1
  fi

  local lane="$1"
  shift
  validate_identifier "$lane" "lane"

  local delete_branch=false
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --delete-branch)
        delete_branch=true
        shift
        ;;
      *)
        echo "Erro: argumento inesperado '$1'." >&2
        exit 1
        ;;
    esac
  done

  local wt_rel_dir=".worktrees/${lane}"
  local wt_abs_dir="${root}/${wt_rel_dir}"

  # Validação de path traversal e existência
  if [[ ! -d "$wt_abs_dir" ]]; then
    echo "Erro: worktree para lane '${lane}' não existe em '${wt_rel_dir}'." >&2
    exit 1
  fi

  local canonical_wt canonical_root
  canonical_wt="$(cd "$wt_abs_dir" && pwd -P)"
  canonical_root="$(cd "${root}/.worktrees" && pwd -P)"
  if [[ "$canonical_wt" != "${canonical_root}/${lane}" ]]; then
    echo "Erro de segurança: tentativa de symlink escape ou path traversal detectada." >&2
    exit 1
  fi

  # Validação 1: Worktree NÃO pode estar sujo
  local wt_dirty
  wt_dirty="$(git -C "$wt_abs_dir" status --porcelain 2>/dev/null || true)"
  if [[ -n "$wt_dirty" ]]; then
    echo "Erro: o worktree '${lane}' possui alterações não commitadas ou arquivos novos (dirty)." >&2
    echo "Remoção bloqueada para evitar perda de dados. O uso de force é estritamente proibido." >&2
    exit 1
  fi

  local branch
  branch="$(git -C "$wt_abs_dir" branch --show-current 2>/dev/null || echo "")"

  # Validação 2: Verificar se a branch possui commits não integrados
  if [[ -n "$branch" ]]; then
    local main_head
    main_head="$(git -C "$root" rev-parse HEAD 2>/dev/null || true)"
    local branch_head
    branch_head="$(git -C "$root" rev-parse "refs/heads/${branch}" 2>/dev/null || true)"

    # Se a branch tem commits além de main_head e não é ancestral de main_head
    if [[ -n "$main_head" && -n "$branch_head" ]]; then
      local is_integrated=false
      if git -C "$root" merge-base --is-ancestor "$branch" "$main_head" 2>/dev/null; then
        is_integrated=true
      fi

      # Checar também se foi integrada na integrationBranch
      local mfile
      mfile="$(manifest_path "$root")"
      local int_branch=""
      if [[ "$is_integrated" == false && -f "$mfile" ]] && command -v python3 >/dev/null 2>&1; then
        int_branch="$(python3 -c "
import json
try:
    with open('$mfile') as f:
        print(json.load(f).get('integrationBranch', ''))
except: pass
")"
        if [[ -n "$int_branch" ]] && git -C "$root" show-ref --verify --quiet "refs/heads/${int_branch}" 2>/dev/null; then
          if git -C "$root" merge-base --is-ancestor "$branch" "refs/heads/${int_branch}" 2>/dev/null; then
            is_integrated=true
          fi
        fi
      fi

      if [[ "$is_integrated" == false ]]; then
        # Verificar se coincide com baseSha registrado no manifesto (ou seja, nenhum commit criado na branch)
        local base_sha=""
        if [[ -f "$mfile" ]] && command -v python3 >/dev/null 2>&1; then
          base_sha="$(python3 -c "
import json
try:
    with open('$mfile') as f:
        data = json.load(f)
    for w in data.get('worktrees', []):
        if w.get('lane') == '$lane':
            print(w.get('baseSha', ''))
            break
except: pass
")"
        fi

        if [[ -n "$base_sha" && "$branch_head" == "$base_sha" ]]; then
          : # Nenhum commit novo foi criado na branch, seguro remover
        else
          echo "Erro: a branch '${branch}' possui commits que NÃO foram integrados ao HEAD ou branch de integração." >&2
          echo "Integre as alterações ou faça merge/cherry-pick antes de remover o worktree." >&2
          exit 1
        fi
      fi
    fi
  fi

  echo "Removendo worktree '${lane}' em '${wt_rel_dir}' (sem force)..."
  # Sem uso de --force
  git -C "$root" worktree remove "$wt_abs_dir"

  if [[ "$delete_branch" == true && -n "$branch" ]]; then
    echo "Removendo branch '${branch}' com segurança (-d)..."
    # git branch -d seguro (recusa se unmerged, sem uso de -D)
    git -C "$root" branch -d "$branch" || {
      echo "Aviso: a branch '${branch}' não pôde ser excluída com 'git branch -d'." >&2
    }
  fi

  # Atualizar manifesto
  local mfile
  mfile="$(manifest_path "$root")"
  if [[ -f "$mfile" ]] && command -v python3 >/dev/null 2>&1; then
    python3 - << PYEOF
import json
mpath = "$mfile"
try:
    with open(mpath, 'r', encoding='utf-8') as f:
        data = json.load(f)
    data["worktrees"] = [w for w in data.get("worktrees", []) if w.get("lane") != "$lane"]
    with open(mpath, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
except Exception:
    pass
PYEOF
  fi

  echo "Worktree '${lane}' removido com sucesso."
}

main() {
  local cmd="${1:-help}"
  shift || true

  case "$cmd" in
    checkpoint)
      cmd_checkpoint "$@"
      ;;
    init-integration)
      cmd_init_integration "$@"
      ;;
    create)
      cmd_create "$@"
      ;;
    list)
      cmd_list "$@"
      ;;
    status)
      cmd_status "$@"
      ;;
    set-task)
      cmd_set_task "$@"
      ;;
    set-lane-meta)
      cmd_set_lane_meta "$@"
      ;;
    validate-ownership)
      cmd_validate_ownership "$@"
      ;;
    remove)
      cmd_remove "$@"
      ;;
    help|--help|-h)
      show_help
      ;;
    *)
      echo "Erro: comando desconhecido '$cmd'." >&2
      echo "Execute 'agent-worktree.sh help' para instruções de uso." >&2
      exit 1
      ;;
  esac
}

main "$@"
