## Why
Títulos reais pararão cedo em serviços de kernel ausentes mesmo com CPU ampliada. Esta lane adiciona serviços prioritários sem tocar integração global.

## What Changes
- Ampliar memória virtual/heap, timers, threads/synchronization e I/O HLE.
- Melhorar registry e diagnóstico de export ausente.
- Garantir handles, buffers e waits bounded/determinísticos.

## Capabilities
### New Capabilities
- `kernel-title-services`: Serviços HLE iniciais exigidos por títulos.

### Modified Capabilities
- `kernel-hle-foundation`: Diagnóstico e dispatch mais completos.

## Impact
Somente `libs/kernel/**`, `tests/unit/test_kernel*` e `libs/kernel/README.md`; sem machine, ABI, root CMake ou UI.