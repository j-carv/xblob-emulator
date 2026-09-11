## Purpose

Define um relógio virtual e uma ordem de eventos reproduzível para coordenar futuros subsistemas sem depender do tempo de parede do host.

## ADDED Requirements

### Requirement: Tempo virtual por ciclos inteiros
O scheduler SHALL representar o tempo em ciclos inteiros monotônicos e MUST NOT depender de relógio de parede para determinar a ordem de execução.

#### Scenario: Avanço explícito
- **WHEN** o scheduler avança por 25 ciclos a partir do ciclo 10
- **THEN** o ciclo atual passa a ser exatamente 35

#### Scenario: Overflow de tempo
- **WHEN** um avanço excederia o maior ciclo representável
- **THEN** a operação falha sem alterar o ciclo atual

### Requirement: Despacho ordenado de eventos
O scheduler SHALL disparar eventos quando seu ciclo alvo for alcançado, em ordem crescente de ciclo e em ordem estável de inserção quando vários eventos compartilharem o mesmo ciclo.

#### Scenario: Eventos simultâneos
- **WHEN** três eventos são agendados para o mesmo ciclo em uma ordem conhecida
- **THEN** os callbacks são executados exatamente nessa ordem

#### Scenario: Evento futuro
- **WHEN** o scheduler avança até um ciclo anterior ao alvo de um evento
- **THEN** o evento permanece pendente e não é executado

### Requirement: Cancelamento determinístico
Eventos pendentes SHALL possuir identificadores opacos canceláveis, e cancelar um evento MUST NOT alterar a ordem relativa dos demais.

#### Scenario: Cancelamento antes do alvo
- **WHEN** um evento pendente é cancelado antes de seu ciclo alvo
- **THEN** ele não é executado e os demais eventos mantêm sua ordem

### Requirement: Execução limitada
O scheduler SHALL permitir executar até um ciclo limite ou até uma quantidade máxima de eventos, evitando laços infinitos causados por reagendamento no mesmo ciclo.

#### Scenario: Evento se reagenda imediatamente
- **WHEN** um callback agenda repetidamente outro evento no ciclo atual e o consumidor define limite de eventos
- **THEN** a execução retorna ao consumidor ao atingir o limite com estado distinguível
