## Why

O emulador monta mídia e prepara `default.xbe`, mas ainda não oferece um loop controlado para observar quanto um título real progride. Este marco integra expansões paralelas de CPU e Kernel HLE em uma experiência experimental segura e mensurável.

## What Changes

- Congelar contratos entre CPU, kernel, machine, ABI e UI antes das lanes.
- Integrar as child changes `expand-ia32-title-execution` e `expand-kernel-title-services`.
- Implementar lifecycle start/pause/resume/step/stop com watchdog e budgets obrigatórios.
- Produzir relatório de compatibilidade com opcode, export, fault, thread, EIP e trace do primeiro bloqueio.
- Expor execução experimental na ABI C e desktop sem bloquear UI.
- Testar somente com fixtures sintéticas; mídia real é entrada local opcional do usuário e nunca entra no repo.

## Capabilities

### New Capabilities
- `experimental-title-execution`: Controle bounded de sessões preparadas e diagnóstico de incompatibilidade.
- `compatibility-diagnostics`: Relatório estruturado de progresso, bloqueios e capacidades ausentes.

### Modified Capabilities
- `machine-session`: Coordenar execução, cancelamento e snapshots thread-safe.
- `desktop-media-inspection`: Oferecer controles experimentais após preparação válida.

## Impact

Integra CPU/kernel ampliados e altera hotspots somente na integration lane: machine, C ABI 1.5, Rust/React, root build/docs. Não promete jogabilidade; mede progresso real de mídia fornecida pelo usuário.