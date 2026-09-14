## Context
Lane GPU do parent `add-interactive-graphics-input-foundation`.

## Goals / Non-Goals
**Goals:** reference 3D subset e unsupported diagnostics.
**Non-Goals:** shaders completos, aceleração host, cycle exact ou game hacks.

## Decisions
- Ownership: `libs/gpu/**`, `tests/unit/test_gpu*`, diary GPU específica, child OpenSpec.
- Denied: root CMake, bus/machine/c_api/apps/kernel/cpu, lockfiles/docs globais.
- Separar render state, vertex fetch, texture, rasterizer e method dispatcher <500 linhas.
- Fixed-point/precisão explicitamente escolhida para golden determinístico; evitar UB/NaN host-dependent.
- Packet/draw valida integralmente ranges e budgets antes de pixel writes.
- Métodos seguem fontes públicas clean-room; desconhecidos agregados, nunca ignorados silenciosamente.

## Testing
Golden frames pequenos, malformed buffers/textures, depth/blend/scissor, determinismo e ASan/UBSan.