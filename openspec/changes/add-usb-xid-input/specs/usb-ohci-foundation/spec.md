## Purpose
Modela OHCI mínimo para enumerar e consultar dispositivo XID sem acessar APIs host.

## ADDED Requirements

### Requirement: Registros OHCI
Controller SHALL expor registros allowlisted, reset, control/status, interrupt mask/status e frame number com masks documentadas.
#### Scenario: Reset
- **WHEN** controller reset é solicitado
- **THEN** filas/IRQs/frame voltam ao estado determinístico inicial

### Requirement: Transfers
Control e interrupt transfers mínimos SHALL validar ED/TD, alignment, links, lengths, cycles e guest ranges antes de efeitos.
#### Scenario: TD cíclico
- **WHEN** descriptors formam ciclo
- **THEN** budget interrompe traversal com fault estruturado

### Requirement: IRQ
Completion/status SHALL gerar IRQ somente quando source e master estão habilitados, com acknowledge W1C.
#### Scenario: IRQ masked
- **WHEN** transfer completa mascarado
- **THEN** status fica pending sem entregar IRQ
