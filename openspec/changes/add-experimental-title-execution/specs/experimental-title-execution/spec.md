## Purpose

Controla execução experimental de mídia preparada sem hangs, races ou alegações prematuras.

## ADDED Requirements

### Requirement: Lifecycle controlado
Sessão preparada SHALL aceitar start, pause, resume, step e stop com transições explícitas e idempotência documentada.

#### Scenario: Pause
- **WHEN** usuário pausa execução ativa
- **THEN** worker alcança safe point bounded e snapshot consistente é retornado

### Requirement: Budgets e watchdog
Toda execução SHALL possuir budgets de instruções, ciclos, wall-time e eventos, com cancelamento cooperativo e sem bloquear thread UI.

#### Scenario: Loop infinito
- **WHEN** guest não progride
- **THEN** watchdog pausa com motivo budget/watchdog e preserva diagnóstico

### Requirement: Entrada local
Mídia SHALL permanecer local sem upload, telemetria ou persistência de conteúdo proprietário em logs.

#### Scenario: Log de fault
- **WHEN** título falha
- **THEN** relatório contém metadados arquiteturais necessários, não bytes arbitrários da mídia
