# Núcleo e Scheduler Determinístico (libs/core)

Este subsistema implementa o relógio virtual monotônico e o agendamento determinístico de eventos do emulador.

## Estado Atual (Marco 2: Ativo)
- Relógio virtual monotônico de 64 bits (`Cycle`), imune a dependências e variações de tempo real do host.
- Fila de eventos ordenada por `(ciclo, sequência)` com desempate estável pela ordem de inserção.
- Agendamento flexível (eventos futuros ou no ciclo corrente) com rejeição estrita de alvos no passado (`ErrorCode::PastCycle`).
- Cancelamento determinístico via `EventId` opaco sem perturbar os eventos restantes.
- Execução controlada via `RunUntil` e `StepCycles` com limite máximo de eventos (`max_events`) para prevenir loops infinitos em reagendamentos imediatos.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`.
- Proibido depender de `libs/cpu` ou `libs/memory`.
