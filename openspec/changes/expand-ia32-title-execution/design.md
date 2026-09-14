## Context
Lane paralela CPU do parent `add-experimental-title-execution`.

## Goals / Non-Goals
**Goals:** instruções prioritárias, REP retomável, #DE, diagnostics.
**Non-Goals:** x87/MMX/SSE completos, JIT, cycle accuracy.

## Decisions
- Ownership exclusivo: `libs/cpu/**`, `tests/unit/test_cpu*`, `libs/cpu/README.md`.
- Denied: root CMake, machine, c_api, kernel, lockfiles, docs globais e parent OpenSpec.
- Reusar pipeline decode/validate/commit e helpers puros de flags.
- Separar famílias em arquivos <500 linhas; nenhum ponteiro host.
- REP executa micro-steps sujeitos ao budget, permitindo IRQ/cancelamento na integração.
- Unsupported metadata fica em tipos CPU locais; integration lane converte ao contrato público.

## Risks / Trade-offs
- Flag edge cases → vetores Intel SDM públicos.
- DIV UB host → aritmética widened e checks antes da operação.
- REP monopolizar → micro-step/budget.

## Testing
CTest focado CPU normal/sanitizers, malformed/truncated/page faults e dupla execução.