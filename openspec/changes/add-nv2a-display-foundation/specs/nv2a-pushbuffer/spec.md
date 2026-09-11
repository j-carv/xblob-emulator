## Purpose

Decodifica e executa um formato inicial de pushbuffer NV2A para workloads sintéticos com limites estritos.

## ADDED Requirements

### Requirement: Fetch defensivo
O processor SHALL ler comandos por endereços guest, validar alinhamento, ranges, contagens e wrap antes de qualquer efeito.

#### Scenario: Packet truncado
- **WHEN** payload termina antes da contagem declarada
- **THEN** nenhum método do packet é aplicado e fault identifica truncamento

### Requirement: Packets e métodos mínimos
O sistema SHALL suportar packets incrementing/non-incrementing e métodos sintéticos allowlisted para bind de surface, clear, fill-rect e flip.

#### Scenario: Clear e fill
- **WHEN** pushbuffer válido limpa surface e desenha retângulo
- **THEN** pixels RGBA esperados são produzidos dentro de bounds

### Requirement: Budgets
Cada submissão SHALL ter limites de words, packets, métodos e ciclos, interrompendo loops/jumps ou trabalho excessivo de forma reproduzível.

#### Scenario: Pushbuffer cíclico
- **WHEN** controle revisita comandos além do budget
- **THEN** processamento pausa com budget-exhausted sem hang

### Requirement: Commit transacional por packet
Validação de packet SHALL preceder seus efeitos; faults não MUST deixar writes parciais daquele packet.

#### Scenario: Rect inválido
- **WHEN** coordenadas excedem surface
- **THEN** framebuffer permanece inalterado para aquele método
