<#
.SYNOPSIS
  agent-worktree.ps1 - Gerenciamento seguro de worktrees Git para agentes paralelos (ACP) no Windows.
.DESCRIPTION
  Gerencia o ciclo de vida de worktrees Git isolados com proibição estrita de force (-f/--force)
  e proteções contra deleção destrutiva de alterações locais ou branches não integradas.
.EXAMPLE
  .\tools\agent-worktree.ps1 create cpu HEAD -Parent m7-core-execution -Change add-cpu-feature -Owned "libs/cpu/**,tests/unit/test_cpu*"
  .\tools\agent-worktree.ps1 checkpoint -Message "feat(cpu): add decoder" -GateCmd "ctest -R test_cpu"
  .\tools\agent-worktree.ps1 init-integration marco-7 HEAD
  .\tools\agent-worktree.ps1 validate-ownership cpu
  .\tools\agent-worktree.ps1 list
  .\tools\agent-worktree.ps1 status cpu
  .\tools\agent-worktree.ps1 set-task cpu -TaskId task-1234
  .\tools\agent-worktree.ps1 remove cpu -DeleteBranch
#>

[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [string]$Command = "help",

  [Parameter(Position = 1)]
  [string]$Lane = "",

  [Parameter(Position = 2)]
  [string]$BaseRef = "HEAD",

  [Parameter()]
  [string]$Parent = "",

  [Parameter()]
  [string]$TaskId = "",

  [Parameter()]
  [string]$Message = "",

  [Parameter()]
  [string]$Change = "",

  [Parameter()]
  [string]$GateCmd = "",

  [Parameter()]
  [string]$Owned = "",

  [Parameter()]
  [string]$Denied = "",

  [Parameter()]
  [string]$DependsOn = "",

  [Parameter()]
  [switch]$DeleteBranch,

  [Parameter()]
  [switch]$Json
)

$ErrorActionPreference = "Stop"

function Show-Help {
@"
Uso:
  agent-worktree.ps1 create <lane> [base-commit] [-Parent <parent>] [-Change <change>] [-Owned <globs>] [-Denied <globs>] [-DependsOn <lanes>]
  agent-worktree.ps1 checkpoint -Message "<mensagem>" [-Change <change>] [-GateCmd "<cmd>"]
  agent-worktree.ps1 init-integration <parent> [base-commit]
  agent-worktree.ps1 validate-ownership <lane> [-BaseRef <base-commit>] [-Json]
  agent-worktree.ps1 list [-Json]
  agent-worktree.ps1 status [lane]
  agent-worktree.ps1 set-task <lane> -TaskId <task-id>
  agent-worktree.ps1 set-lane-meta <lane> [-Change <change>] [-Owned <globs>] [-Denied <globs>] [-DependsOn <lanes>]
  agent-worktree.ps1 remove <lane> [-DeleteBranch]
  agent-worktree.ps1 help

Subcomandos:
  create              Cria um worktree isolado em .worktrees/<lane> para a lane informada.
                      Requer obrigatoriamente que o repositório principal esteja limpo.
  checkpoint          Executa commit explícito de staging com preflight de gates e OpenSpec.
                      Exige mensagem (-Message), recusa staging vazio e NUNCA faz push automático.
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
"@
}

function Get-RepoRoot {
  $root = git rev-parse --show-toplevel 2>$null
  if (-not $root) {
    Write-Error "Erro: não foi possível localizar a raiz do repositório Git."
    exit 1
  }
  return $root.Trim()
}

function Assert-ValidIdentifier([string]$value, [string]$fieldName) {
  if ($value -notmatch '^[a-zA-Z0-9_-]+$') {
    Write-Error "Erro: identificador inválido para $fieldName: '$value'. Use apenas [a-zA-Z0-9_-]."
    exit 1
  }
}

function Get-ManifestPath([string]$root) {
  return Join-Path $root ".worktrees\manifest.json"
}

function Initialize-Manifest([string]$root) {
  $mpath = Get-ManifestPath $root
  $dir = Split-Path $mpath -Parent
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
  }
  if (-not (Test-Path $mpath)) {
    @{
      schemaVersion = "2.0"
      version = "2.0"
      parent = ""
      integrationBranch = ""
      baseSha = ""
      worktrees = @()
    } | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
  } else {
    try {
      $raw = Get-Content -Path $mpath -Raw -Encoding UTF8
      $data = $raw | ConvertFrom-Json
      $changed = $false
      if ($data.schemaVersion -ne "2.0" -or $data.version -ne "2.0") {
        $data.schemaVersion = "2.0"
        $data.version = "2.0"
        $changed = $true
      }
      if (-not (Get-Member -InputObject $data -Name "parent")) {
        $data | Add-Member -MemberType NoteProperty -Name "parent" -Value ""
        $changed = $true
      }
      if (-not (Get-Member -InputObject $data -Name "integrationBranch")) {
        $data | Add-Member -MemberType NoteProperty -Name "integrationBranch" -Value ""
        $changed = $true
      }
      if (-not (Get-Member -InputObject $data -Name "baseSha")) {
        $data | Add-Member -MemberType NoteProperty -Name "baseSha" -Value ""
        $changed = $true
      }
      if ($changed) {
        $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
      }
    } catch {}
  }
}

function Match-Glob([string]$filePath, [string]$pattern) {
  $normPath = $filePath.Trim().Replace('\', '/')
  $normPat = $pattern.Trim().Replace('\', '/')
  if ([string]::IsNullOrWhiteSpace($normPat)) { return $false }

  if ($normPat.EndsWith("/**")) {
    $prefix = $normPat.Substring(0, $normPat.Length - 3).TrimEnd('/')
    if ($normPath -eq $prefix -or $normPath.StartsWith("$prefix/")) {
      return $true
    }
  }

  return ($normPath -like $normPat)
}

function Invoke-Checkpoint {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Message)) {
    Write-Error "Erro: mensagem de commit obrigatória para o checkpoint. Use -Message 'mensagem'."
    exit 1
  }

  # Validação 1: Staging não pode estar vazio (auto-stage indiscriminado proibido)
  git -C $root diff --cached --quiet 2>$null
  if ($LASTEXITCODE -eq 0) {
    Write-Error "Erro: nenhuma alteração em staging para commit. Adicione os arquivos desejados com 'git add' antes de invocar checkpoint (auto-staging indiscriminado é proibido por governança)."
    exit 1
  }

  Write-Host "=== Revisão de Arquivos em Staging ==="
  git -C $root diff --cached --name-status

  # Validação 2: Preflight OpenSpec
  if (-not [string]::IsNullOrWhiteSpace($Change)) {
    $openspecCmd = Get-Command openspec -ErrorAction SilentlyContinue
    if ($openspecCmd) {
      Write-Host "Executando preflight OpenSpec strict para '$Change'..."
      Push-Location $root
      try {
        & openspec validate $Change --strict
        if ($LASTEXITCODE -ne 0) {
          Write-Error "Erro: preflight OpenSpec strict falhou para a change '$Change'. Checkpoint abortado."
          exit 1
        }
      } finally {
        Pop-Location
      }
    }
  }

  # Validação 3: Preflight de Gates
  if (-not [string]::IsNullOrWhiteSpace($GateCmd)) {
    Write-Host "Executando gate preflight configurado: $GateCmd..."
    Push-Location $root
    try {
      Invoke-Expression $GateCmd
      if ($LASTEXITCODE -ne 0) {
        Write-Error "Erro: o gate de verificação preflight falhou. Checkpoint abortado."
        exit 1
      }
    } finally {
      Pop-Location
    }
  }

  Write-Host "Criando commit de checkpoint..."
  git -C $root commit -m $Message
  if ($LASTEXITCODE -ne 0) {
    Write-Error "Erro ao executar git commit."
    exit 1
  }
  $newSha = (git -C $root rev-parse --short HEAD 2>$null).Trim()
  Write-Host "Checkpoint criado com sucesso no commit $newSha. (Push automático desabilitado por governança)."
}

function Invoke-InitIntegration {
  $root = Get-RepoRoot
  $targetParent = if ($Parent) { $Parent } else { $Lane }
  if ([string]::IsNullOrWhiteSpace($targetParent)) {
    Write-Error "Erro: informe o identificador da parent change. Exemplo: .\tools\agent-worktree.ps1 init-integration marco-7"
    exit 1
  }

  Assert-ValidIdentifier $targetParent "parent"

  # Validação 1: Workspace principal DEVE estar rigorosamente limpo
  $dirty = git -C $root status --porcelain 2>$null
  if ($dirty) {
    Write-Error "Erro: o workspace principal possui alterações não commitadas (dirty). A inicialização da branch de integração requer base limpa."
    exit 1
  }

  # Validação 2: Base commit válida
  $baseSha = git -C $root rev-parse --verify "${BaseRef}^{commit}" 2>$null
  if (-not $baseSha) {
    Write-Error "Erro: commit base inválido '$BaseRef'."
    exit 1
  }
  $baseSha = $baseSha.Trim()

  $branch = "integration/$targetParent"

  # Validação 3: Se branch já existe, verificar se diverge de BASE_SHA
  git -C $root show-ref --verify --quiet "refs/heads/$branch" 2>$null
  if ($LASTEXITCODE -eq 0) {
    $existingSha = (git -C $root rev-parse "refs/heads/$branch" 2>$null).Trim()
    if ($existingSha -ne $baseSha) {
      $shortExt = if ($existingSha.Length -ge 8) { $existingSha.Substring(0, 8) } else { $existingSha }
      $shortBase = if ($baseSha.Length -ge 8) { $baseSha.Substring(0, 8) } else { $baseSha }
      Write-Error "Erro: a branch '$branch' já existe em '$shortExt' e diverge do BASE_SHA informado '$shortBase'."
      exit 1
    } else {
      Write-Host "A branch '$branch' já existe e coincide com o commit base."
    }
  } else {
    git -C $root branch $branch $baseSha
    if ($LASTEXITCODE -ne 0) {
      Write-Error "Erro ao criar branch de integração '$branch'."
      exit 1
    }
    $shortBase = if ($baseSha.Length -ge 8) { $baseSha.Substring(0, 8) } else { $baseSha }
    Write-Host "Branch '$branch' criada com sucesso a partir de $shortBase."
  }

  Initialize-Manifest $root
  $mpath = Get-ManifestPath $root
  try {
    $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
  } catch {
    $data = [PSCustomObject]@{ schemaVersion = "2.0"; version = "2.0"; worktrees = @() }
  }

  $data.parent = $targetParent
  $data.integrationBranch = $branch
  $data.baseSha = $baseSha

  $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
  Write-Host "Integração inicializada para parent '$targetParent' na branch '$branch'."
}

function Invoke-Create {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane)) {
    Write-Error "Erro: informe o nome da lane. Exemplo: .\tools\agent-worktree.ps1 create cpu"
    exit 1
  }

  Assert-ValidIdentifier $Lane "lane"

  Initialize-Manifest $root
  $mpath = Get-ManifestPath $root
  $manifestParent = ""
  if (Test-Path $mpath) {
    try {
      $mdata = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
      if ($mdata.parent) { $manifestParent = $mdata.parent }
    } catch {}
  }

  $targetParent = if ($Parent) { $Parent } else { $manifestParent }
  if ($targetParent) {
    Assert-ValidIdentifier $targetParent "parent"
  }

  # Validação 1: Workspace principal DEVE estar rigorosamente limpo
  $dirty = git -C $root status --porcelain 2>$null
  if ($dirty) {
    Write-Error "Erro: o workspace principal possui alterações não commitadas (dirty). A criação de worktrees requer base limpa e validada."
    exit 1
  }

  # Validação 2: Base commit válida
  $baseSha = git -C $root rev-parse --verify "${BaseRef}^{commit}" 2>$null
  if (-not $baseSha) {
    Write-Error "Erro: commit base inválido '$BaseRef'."
    exit 1
  }
  $baseSha = $baseSha.Trim()

  $wtRelDir = ".worktrees\$Lane"
  $wtAbsDir = [System.IO.Path]::GetFullPath((Join-Path $root $wtRelDir))
  $expectedBaseDir = [System.IO.Path]::GetFullPath((Join-Path $root ".worktrees"))

  # Validação 3: Path escape
  if (-not $wtAbsDir.StartsWith($expectedBaseDir, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Erro de segurança: tentativa de path traversal detectada para o diretório da lane."
    exit 1
  }

  if (Test-Path $wtAbsDir) {
    Write-Error "Erro: diretório de worktree já existe em '$wtRelDir'."
    exit 1
  }

  $branch = if ($targetParent) { "agent/$targetParent/$Lane" } else { "agent/$Lane" }

  # Validação 4: Branch já existe?
  git -C $root show-ref --verify --quiet "refs/heads/$branch" 2>$null
  if ($LASTEXITCODE -eq 0) {
    Write-Error "Erro: a branch '$branch' já existe."
    exit 1
  }

  Write-Host "Criando worktree '$Lane' em '$wtRelDir' na branch '$branch'..."
  # Execução estritamente sem --force
  git -C $root worktree add -b $branch $wtAbsDir $baseSha
  if ($LASTEXITCODE -ne 0) {
    Write-Error "Erro ao executar git worktree add."
    exit 1
  }

  try {
    $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
  } catch {
    $data = [PSCustomObject]@{ schemaVersion = "2.0"; version = "2.0"; worktrees = @() }
  }

  $data.schemaVersion = "2.0"
  $data.version = "2.0"

  $ownedList = if ($Owned) { ($Owned -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ } } else { @() }
  $deniedList = if ($Denied) { ($Denied -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ } } else { @() }
  $depsList = if ($DependsOn) { ($DependsOn -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ } } else { @() }

  $now = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
  $list = [System.Collections.Generic.List[object]]@()
  if ($data.worktrees) {
    foreach ($w in $data.worktrees) {
      if ($w.lane -ne $Lane) {
        $list.Add($w)
      }
    }
  }

  $list.Add([PSCustomObject]@{
    lane = $Lane
    parent = $targetParent
    branch = $branch
    path = ".worktrees/$Lane"
    baseSha = $baseSha
    createdAt = $now
    taskId = ""
    change = $Change
    owned = $ownedList
    denied = $deniedList
    dependencies = $depsList
  })

  $data.worktrees = $list.ToArray()
  $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8

  Write-Host "Worktree criado com sucesso em '$wtRelDir'."
}

function Invoke-ValidateOwnership {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane)) {
    Write-Error "Erro: informe o nome da lane. Exemplo: .\tools\agent-worktree.ps1 validate-ownership cpu"
    exit 1
  }

  Assert-ValidIdentifier $Lane "lane"

  Initialize-Manifest $root
  $mpath = Get-ManifestPath $root
  $laneData = $null
  if (Test-Path $mpath) {
    try {
      $mdata = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
      foreach ($w in $mdata.worktrees) {
        if ($w.lane -eq $Lane) {
          $laneData = $w
          break
        }
      }
    } catch {}
  }

  $branch = if ($laneData -and $laneData.branch) { $laneData.branch } else { "agent/$Lane" }
  $baseSha = if ($BaseRef -ne "HEAD") {
    $BaseRef
  } elseif ($laneData -and $laneData.baseSha) {
    $laneData.baseSha
  } else {
    $mb = git -C $root merge-base $branch HEAD 2>$null
    if ($mb) { $mb.Trim() } else { "HEAD" }
  }

  $defaultHotspots = @(
    "CMakeLists.txt",
    "libs/c_api/**",
    "libs/machine/**",
    "apps/desktop/package-lock.json",
    "Cargo.lock",
    "AGENTS.md",
    "README.md",
    "ARCHITECTURE.md"
  )

  $ownedPatterns = if ($laneData -and $laneData.owned) { @($laneData.owned) } else { @() }
  $deniedPatterns = [System.Collections.Generic.List[string]]@()
  foreach ($h in $defaultHotspots) { $deniedPatterns.Add($h) }
  if ($laneData -and $laneData.denied) {
    foreach ($d in $laneData.denied) { $deniedPatterns.Add($d) }
  }

  $diffFiles = [System.Collections.Generic.HashSet[string]]@()
  $outDiff = git -C $root diff --name-only "$baseSha..$branch" 2>$null
  if ($outDiff) {
    foreach ($line in ($outDiff -split "`r?`n")) {
      if ($line.Trim()) { [void]$diffFiles.Add($line.Trim().Replace('\', '/')) }
    }
  }

  $wtAbsDir = Join-Path $root ".worktrees\$Lane"
  if (Test-Path $wtAbsDir) {
    $wtStatus = git -C $wtAbsDir status --porcelain 2>$null
    if ($wtStatus) {
      foreach ($line in ($wtStatus -split "`r?`n")) {
        if ($line.Length -ge 3) {
          $p = $line.Substring(3).Trim()
          if ($p -match ' -> ') { $p = ($p -split ' -> ')[1] }
          [void]$diffFiles.Add($p.Trim().Replace('\', '/'))
        }
      }
    }
  }

  $changedList = [System.Linq.Enumerable]::ToArray([System.Linq.Enumerable]::OrderBy($diffFiles, [System.Func[string,string]]{ param($x) $x }))
  $violations = [System.Collections.Generic.List[object]]@()

  foreach ($f in $changedList) {
    $isDenied = $false
    foreach ($pat in $deniedPatterns) {
      if (Match-Glob $f $pat) {
        $violations.Add([PSCustomObject]@{
          file = $f
          reason = "denied_hotspot"
          pattern = $pat
        })
        $isDenied = $true
        break
      }
    }
    if ($isDenied) { continue }

    if ($ownedPatterns.Count -gt 0) {
      $isOwned = $false
      foreach ($pat in $ownedPatterns) {
        if (Match-Glob $f $pat) {
          $isOwned = $true
          break
        }
      }
      if (-not $isOwned) {
        $violations.Add([PSCustomObject]@{
          file = $f
          reason = "outside_ownership"
          pattern = ""
        })
      }
    }
  }

  $isValid = ($violations.Count -eq 0)

  if ($Json) {
    [PSCustomObject]@{
      lane = $Lane
      branch = $branch
      baseSha = $baseSha
      valid = $isValid
      totalChangedFiles = $changedList.Length
      changedFiles = $changedList
      ownedPatterns = $ownedPatterns
      deniedPatterns = $deniedPatterns.ToArray()
      violations = $violations.ToArray()
    } | ConvertTo-Json -Depth 5
  } else {
    $shortSha = if ($baseSha.Length -ge 8) { $baseSha.Substring(0, 8) } else { $baseSha }
    Write-Host "=== Relatório de Ownership da Lane '$Lane' ==="
    Write-Host "Branch: $branch"
    Write-Host "Base SHA: $shortSha"
    Write-Host "Total de arquivos modificados: $($changedList.Length)"
    if ($changedList.Length -gt 0) {
      Write-Host "Arquivos detectados:"
      foreach ($f in $changedList) { Write-Host "  - $f" }
    } else {
      Write-Host "  (Nenhum arquivo alterado em relação à base)"
    }

    if ($violations.Count -gt 0) {
      Write-Host "`nVIOLAÇÕES DE OWNERSHIP / HOTSPOTS ENCONTRADAS:"
      foreach ($v in $violations) {
        if ($v.reason -eq "denied_hotspot") {
          Write-Host "  [BLOQUEADO - HOTSPOT/DENIED] '$($v.file)' (coincide com padrão '$($v.pattern)')"
        } else {
          Write-Host "  [BLOQUEADO - FORA DE ESCOPO] '$($v.file)' (fora dos padrões owned declarados)"
        }
      }
      Write-Host "`nResultado: REJEITADO (violação do protocolo de ownership disjunto)"
    } else {
      Write-Host "`nResultado: APROVADO (nenhuma violação de ownership ou hotspot)"
    }
  }

  if (-not $isValid) {
    exit 1
  }
}

function Invoke-List {
  $root = Get-RepoRoot
  $mpath = Get-ManifestPath $root
  Initialize-Manifest $root

  if ($Json) {
    if (Test-Path $mpath) {
      Get-Content -Path $mpath -Raw -Encoding UTF8
    } else {
      Write-Output "{}"
    }
    return
  }

  Write-Host "=== Worktrees Git Registrados ==="
  git -C $root worktree list

  if (Test-Path $mpath) {
    Write-Host "`n=== Manifesto Local (.worktrees/manifest.json) ==="
    try {
      $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
      $par = if ($data.parent) { $data.parent } else { "(nenhum)" }
      $intB = if ($data.integrationBranch) { $data.integrationBranch } else { "(nenhuma)" }
      Write-Host "SchemaVersion: $($data.schemaVersion) | Parent: $par | Integration: $intB"

      if (-not $data.worktrees -or $data.worktrees.Count -eq 0) {
        Write-Host "Nenhuma lane registrada no manifesto."
      } else {
        foreach ($w in $data.worktrees) {
          $tid = if ($w.taskId) { $w.taskId } else { "(nenhum)" }
          $shortSha = if ($w.baseSha.Length -ge 8) { $w.baseSha.Substring(0, 8) } else { $w.baseSha }
          $deps = if ($w.dependencies) { ($w.dependencies -join ',') } else { "none" }
          $ownedStr = if ($w.owned) { ($w.owned -join ',') } else { "all" }
          Write-Host "- Lane: $($w.lane) | Branch: $($w.branch) | Base: $shortSha | TaskId: $tid | Deps: [$deps] | Owned: [$ownedStr]"
        }
      }
    } catch {
      Write-Host "Erro ao ler manifesto: $_"
    }
  }
}

function Invoke-Status {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane)) {
    Invoke-List
    return
  }

  Assert-ValidIdentifier $Lane "lane"
  $wtRelDir = ".worktrees\$Lane"
  $wtAbsDir = Join-Path $root $wtRelDir

  if (-not (Test-Path $wtAbsDir)) {
    Write-Error "Erro: worktree para lane '$Lane' não encontrado em '$wtRelDir'."
    exit 1
  }

  Write-Host "=== Status da Lane '$Lane' ==="
  Write-Host "Diretório: $wtRelDir"
  $curBranch = git -C $wtAbsDir branch --show-current 2>$null
  Write-Host "Branch: $curBranch"
  $curCommit = git -C $wtAbsDir rev-parse --short HEAD 2>$null
  Write-Host "HEAD: $curCommit"

  $wtDirty = git -C $wtAbsDir status --porcelain 2>$null
  if ($wtDirty) {
    Write-Host "Estado: MODIFICADO / DIRTY"
    Write-Host $wtDirty
  } else {
    Write-Host "Estado: LIMPO"
  }

  $mainHead = git -C $root rev-parse HEAD 2>$null
  if ($curBranch -and $mainHead) {
    git -C $root merge-base --is-ancestor $curBranch $mainHead 2>$null
    if ($LASTEXITCODE -eq 0) {
      $shortMain = $mainHead.Trim().Substring(0, 8)
      Write-Host "Integração (HEAD principal): INTEGRADO ($shortMain)"
    } else {
      $unmergedCount = git -C $root rev-list --count "$($mainHead.Trim())..$($curBranch.Trim())" 2>$null
      Write-Host "Integração (HEAD principal): PENDENTE ($($unmergedCount.Trim()) commit(s) à frente)"
    }
  }
}

function Invoke-SetTask {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane) -or [string]::IsNullOrWhiteSpace($TaskId)) {
    Write-Error "Erro: informe -Lane e -TaskId. Exemplo: .\tools\agent-worktree.ps1 set-task cpu -TaskId task-1234"
    exit 1
  }

  Assert-ValidIdentifier $Lane "lane"
  Initialize-Manifest $root
  $mpath = Get-ManifestPath $root

  try {
    $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
  } catch {
    $data = [PSCustomObject]@{ schemaVersion = "2.0"; version = "2.0"; worktrees = @() }
  }

  $found = $false
  if ($data.worktrees) {
    foreach ($w in $data.worktrees) {
      if ($w.lane -eq $Lane) {
        $w.taskId = $TaskId
        $found = $true
        break
      }
    }
  }

  if (-not $found) {
    $list = [System.Collections.Generic.List[object]]@()
    if ($data.worktrees) {
      foreach ($w in $data.worktrees) { $list.Add($w) }
    }
    $list.Add([PSCustomObject]@{
      lane = $Lane
      parent = ""
      branch = ""
      path = ".worktrees/$Lane"
      baseSha = ""
      createdAt = ""
      taskId = $TaskId
      change = ""
      owned = @()
      denied = @()
      dependencies = @()
    })
    $data.worktrees = $list.ToArray()
  }

  $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
  Write-Host "Task ID '$TaskId' registrado para lane '$Lane'."
}

function Invoke-SetLaneMeta {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane)) {
    Write-Error "Erro: informe a lane. Exemplo: .\tools\agent-worktree.ps1 set-lane-meta cpu -Change feat-cpu"
    exit 1
  }

  Assert-ValidIdentifier $Lane "lane"
  Initialize-Manifest $root
  $mpath = Get-ManifestPath $root

  try {
    $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
  } catch {
    $data = [PSCustomObject]@{ schemaVersion = "2.0"; version = "2.0"; worktrees = @() }
  }

  $found = $false
  if ($data.worktrees) {
    foreach ($w in $data.worktrees) {
      if ($w.lane -eq $Lane) {
        $found = $true
        if ($Change) { $w.change = $Change }
        if ($Owned) {
          $w.owned = ($Owned -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }
        }
        if ($Denied) {
          $w.denied = ($Denied -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }
        }
        if ($DependsOn) {
          $w.dependencies = ($DependsOn -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }
        }
        break
      }
    }
  }

  if (-not $found) {
    Write-Error "Erro: lane '$Lane' não encontrada no manifesto."
    exit 1
  }

  $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
  Write-Host "Metadados atualizados com sucesso para lane '$Lane'."
}

function Invoke-Remove {
  $root = Get-RepoRoot
  if ([string]::IsNullOrWhiteSpace($Lane)) {
    Write-Error "Erro: informe a lane a remover. Exemplo: .\tools\agent-worktree.ps1 remove cpu"
    exit 1
  }

  Assert-ValidIdentifier $Lane "lane"
  $wtRelDir = ".worktrees\$Lane"
  $wtAbsDir = [System.IO.Path]::GetFullPath((Join-Path $root $wtRelDir))
  $expectedBaseDir = [System.IO.Path]::GetFullPath((Join-Path $root ".worktrees"))

  if (-not (Test-Path $wtAbsDir)) {
    Write-Error "Erro: worktree para lane '$Lane' não existe em '$wtRelDir'."
    exit 1
  }

  # Validação de integridade e path escape
  if (-not $wtAbsDir.StartsWith($expectedBaseDir, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Erro de segurança: tentativa de path traversal detectada."
    exit 1
  }

  # Validação 1: Worktree NÃO pode estar sujo
  $wtDirty = git -C $wtAbsDir status --porcelain 2>$null
  if ($wtDirty) {
    Write-Error "Erro: o worktree '$Lane' possui alterações não commitadas ou arquivos novos (dirty). Remoção bloqueada para evitar perda de dados. Force é proibido."
    exit 1
  }

  $branch = (git -C $wtAbsDir branch --show-current 2>$null).Trim()

  # Validação 2: Verificar commits não integrados
  if ($branch) {
    $mainHead = (git -C $root rev-parse HEAD 2>$null).Trim()
    $branchHead = (git -C $root rev-parse "refs/heads/$branch" 2>$null).Trim()

    if ($mainHead -and $branchHead) {
      $isIntegrated = $false
      git -C $root merge-base --is-ancestor $branch $mainHead 2>$null
      if ($LASTEXITCODE -eq 0) {
        $isIntegrated = $true
      }

      $mpath = Get-ManifestPath $root
      if (-not $isIntegrated -and (Test-Path $mpath)) {
        try {
          $mdata = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
          if ($mdata.integrationBranch) {
            git -C $root show-ref --verify --quiet "refs/heads/$($mdata.integrationBranch)" 2>$null
            if ($LASTEXITCODE -eq 0) {
              git -C $root merge-base --is-ancestor $branch "refs/heads/$($mdata.integrationBranch)" 2>$null
              if ($LASTEXITCODE -eq 0) {
                $isIntegrated = $true
              }
            }
          }
        } catch {}
      }

      if (-not $isIntegrated) {
        # Verificar se a branch nunca avançou além do baseSha
        $baseSha = ""
        if (Test-Path $mpath) {
          try {
            $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
            foreach ($w in $data.worktrees) {
              if ($w.lane -eq $Lane) {
                $baseSha = $w.baseSha
                break
              }
            }
          } catch {}
        }

        if ($baseSha -and $branchHead -eq $baseSha) {
          # Nenhum commit novo foi feito na branch
        } else {
          Write-Error "Erro: a branch '$branch' possui commits que NÃO foram integrados ao HEAD principal ou branch de integração. Integre as alterações antes de remover o worktree."
          exit 1
        }
      }
    }
  }

  Write-Host "Removendo worktree '$Lane' em '$wtRelDir' (sem force)..."
  # Execução estritamente sem --force
  git -C $root worktree remove $wtAbsDir
  if ($LASTEXITCODE -ne 0) {
    Write-Error "Erro ao remover worktree via git worktree remove."
    exit 1
  }

  if ($DeleteBranch -and $branch) {
    Write-Host "Removendo branch '$branch' com segurança (-d)..."
    git -C $root branch -d $branch
  }

  # Atualizar manifesto
  $mpath = Get-ManifestPath $root
  if (Test-Path $mpath) {
    try {
      $data = Get-Content -Path $mpath -Raw -Encoding UTF8 | ConvertFrom-Json
      $list = [System.Collections.Generic.List[object]]@()
      if ($data.worktrees) {
        foreach ($w in $data.worktrees) {
          if ($w.lane -ne $Lane) { $list.Add($w) }
        }
      }
      $data.worktrees = $list.ToArray()
      $data | ConvertTo-Json -Depth 5 | Set-Content -Path $mpath -Encoding UTF8
    } catch {}
  }

  Write-Host "Worktree '$Lane' removido com sucesso."
}

switch ($Command.ToLower()) {
  "checkpoint"          { Invoke-Checkpoint }
  "init-integration"    { Invoke-InitIntegration }
  "create"              { Invoke-Create }
  "list"                { Invoke-List }
  "status"              { Invoke-Status }
  "set-task"            { Invoke-SetTask }
  "set-lane-meta"       { Invoke-SetLaneMeta }
  "validate-ownership"  { Invoke-ValidateOwnership }
  "remove"              { Invoke-Remove }
  "help"                { Show-Help }
  "--help"              { Show-Help }
  "-h"                  { Show-Help }
  default {
    Write-Error "Erro: comando desconhecido '$Command'. Execute 'agent-worktree.ps1 help' para instruções."
    exit 1
  }
}
