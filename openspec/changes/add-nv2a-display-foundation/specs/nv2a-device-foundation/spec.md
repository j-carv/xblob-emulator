## Purpose

Modela um subconjunto inicial clean-room do NV2A com estado e MMIO seguros.

## ADDED Requirements

### Requirement: Registros allowlisted
O dispositivo SHALL aceitar apenas registros e widths implementados, aplicando máscaras read/write e retornando unsupported para métodos desconhecidos.

#### Scenario: Registro conhecido
- **WHEN** guest escreve registrador implementado
- **THEN** somente bits graváveis alteram estado e leitura reflete a máscara documentada

### Requirement: Lifecycle e reset
Reset SHALL restaurar estado, filas, faults e superfície a valores determinísticos sem callbacks pendentes.

#### Scenario: Reset após comandos
- **WHEN** GPU é resetada
- **THEN** estado coincide byte a byte com instância nova

### Requirement: Eventos determinísticos
Operações assíncronas SHALL ser agendadas por ciclos guest e sequência estável, produzindo IRQ apenas por fonte habilitada.

#### Scenario: Frame completo
- **WHEN** flip termina no ciclo agendado
- **THEN** status e IRQ tornam-se observáveis exatamente nessa fronteira

### Requirement: Proveniência
Implementação SHALL citar documentação pública/clean-room e MUST NOT incorporar microcode, drivers ou headers proprietários.

#### Scenario: Auditoria
- **WHEN** fontes e fixtures são revisadas
- **THEN** somente constantes justificadas e dados sintéticos estão presentes
