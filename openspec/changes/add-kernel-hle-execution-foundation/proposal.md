## Why

A máquina já prepara XBEs sintéticos em memória paginada, mas o subconjunto mínimo de CPU e a ausência de exceções e serviços de sistema impedem programas diagnósticos realistas. O Marco 4 deve criar uma base de execução controlada e auditável antes de GPU, áudio ou títulos comerciais.

## What Changes

- Expandir o intérprete IA-32 com decodificação ModR/M e instruções fundamentais de dados, pilha, fluxo, aritmética e flags.
- Implementar entrega determinística de exceções e interrupções com IDT/gates em escopo protegido de 32 bits.
- Criar `libs/kernel` como HLE clean-room, com registry de exports e serviços iniciais de debug, heap, threads e sincronização.
- Integrar thunks/imports sintéticos do loader ao dispatcher HLE sem executar ou distribuir `xboxkrnl.exe`.
- Executar programas diagnósticos sintéticos end-to-end com budgets, traces estruturados e resultados reproduzíveis.
- Estender ABI/UI apenas com diagnóstico e trace, mantendo execução comercial e controles Play fora de escopo.

## Capabilities

### New Capabilities
- `x86-interpreter-core`: Subconjunto IA-32 útil, decodificação ModR/M, operandos e flags verificáveis.
- `x86-exception-interrupts`: Modelo inicial de faults, traps, IDT e entrega de interrupções determinística.
- `kernel-hle-foundation`: Dispatcher e serviços fundamentais clean-room de kernel para guests sintéticos.
- `diagnostic-execution`: Execução headless controlada de programas sintéticos com trace e budgets.

### Modified Capabilities
- `xbe-image-loader`: Planejar e materializar uma import thunk table sintética validada.
- `machine-session`: Coordenar CPU, interrupções e kernel HLE preservando lifecycle explícito.

## Impact

Novos componentes em `libs/kernel`; amplia `cpu`, `loader`, `machine`, ABI C e diagnóstico desktop. Usa somente documentação pública e fixtures geradas. Não implementa kernel oficial, syscalls completas, SMP, modo real/v86, GPU, áudio nem suporte a jogos.