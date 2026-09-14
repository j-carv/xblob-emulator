## Context
Lane input do parent `add-interactive-graphics-input-foundation`.

## Goals / Non-Goals
**Goals:** OHCI subset, XID reports, hotplug, deterministic snapshots.
**Non-Goals:** APIs host, force feedback, hubs complexos, USB completo ou game hacks.

## Decisions
- Ownership: `libs/usb/**`, `libs/input/**`, `tests/unit/test_usb*`, `tests/unit/test_input*`, diary própria e child OpenSpec.
- Denied: root CMake, bus/machine/c_api/apps/GPU/kernel/cpu e docs globais.
- `HostInputSnapshot` é estrutura domínio neutra; integração host ocorre depois.
- OHCI decodifica descriptors byte a byte via memory contracts, visited/budgets e commit transacional.
- XID é `UsbDevice`; reports little-endian fixed-width, sequence monotônica.
- IRQ exposta por interface estreita adaptada na integration lane.

## Risks / Trade-offs
- OHCI amplo → somente requests/transfers necessários e unsupported honesto.
- Layout XID → fontes públicas clean-room e fixtures.
- Input race → snapshots imutáveis/sequence.

## Testing
Descriptors sintéticos, cycles/ranges, buttons/axes/triggers, hotplug, IRQ e determinismo.