## Purpose
Fornece serviços HLE determinísticos necessários ao progresso inicial de títulos.

## ADDED Requirements

### Requirement: Memória e heap
Kernel SHALL oferecer allocate/free/query de virtual memory e heap com alignment/protection/range checked e status guest estáveis.
#### Scenario: Commit inválido
- **WHEN** range sobrepõe região reservada ou overflowa
- **THEN** operação falha atomicamente

### Requirement: Timers e tempo
Kernel SHALL expor tick/system/performance time determinísticos e timers one-shot/periodic ligados ao scheduler guest.
#### Scenario: Timer periódico
- **WHEN** ciclos avançam além de múltiplos períodos
- **THEN** expirations seguem política documentada sem drift host

### Requirement: Threads e sincronização
Thread create/exit/yield, events, semaphores, mutants e waits single/multiple SHALL usar handles geracionais, timeout e wake order estável.
#### Scenario: Wait timeout
- **WHEN** objeto não sinaliza antes do deadline
- **THEN** thread acorda uma vez com timeout no ciclo correto

### Requirement: I/O ampliado
File/device services SHALL suportar padrões síncronos prioritários e modelar pending/completion somente quando realmente implementado.
#### Scenario: Unsupported async
- **WHEN** forma async não possui completion path
- **THEN** retorna unsupported sem perder callback/handle

### Requirement: Diagnóstico de exports
Ordinal ausente SHALL produzir metadata bounded com ordinal, thread, EIP, count e argumentos validados disponíveis.
#### Scenario: Unknown ordinal
- **WHEN** thunk não registrado é chamado
- **THEN** kernel retorna stop estruturado sem crash ou salto arbitrário
