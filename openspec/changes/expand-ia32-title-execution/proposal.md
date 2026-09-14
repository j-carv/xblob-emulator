## Why
XBEs reais usarão instruções além do subconjunto atual. Esta lane reduz paradas por opcode sem tocar contratos globais.

## What Changes
- Implementar shifts/rotates, multiply/divide, bit/extension, string e atomic primitives prioritárias.
- Melhorar decoder/prefixes e diagnósticos unsupported.
- Adicionar vetores de borda e regressão determinística.

## Capabilities
### New Capabilities
- `ia32-title-instructions`: Instruções IA-32 adicionais para execução inicial de títulos.

### Modified Capabilities
- `x86-interpreter-core`: Diagnóstico preciso de forms ainda não suportadas.

## Impact
Somente `libs/cpu/**`, `tests/unit/test_cpu*` e `libs/cpu/README.md`. Não altera machine, ABI, CMake raiz ou kernel.