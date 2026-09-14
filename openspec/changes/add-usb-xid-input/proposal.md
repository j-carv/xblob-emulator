## Why
Títulos precisam enxergar controles Xbox; hoje não há USB ou XID no guest.

## What Changes
- Criar USB OHCI mínimo e dispositivo XID.
- Normalizar snapshots host de gamepad/teclado por contrato abstrato.
- Entregar reports e IRQ deterministicamente com testes sintéticos.

## Capabilities
### New Capabilities
- `usb-ohci-foundation`: Host controller USB inicial e transfers bounded.
- `xid-controller`: Controle Xbox guest com reports de input.

### Modified Capabilities
_Nenhuma._

## Impact
Cria `libs/usb` e `libs/input` e testes próprios; integração machine/UI fica na parent lane.