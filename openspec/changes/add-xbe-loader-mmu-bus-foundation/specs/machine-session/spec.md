## Purpose

Compõe subsistemas em uma sessão headless com ownership e lifecycle explícitos, oferecendo uma base testável para kernel e dispositivos futuros.

## ADDED Requirements

### Requirement: Lifecycle explícito
A sessão SHALL distinguir created, prepared, paused, faulted e stopped, rejeitando transições inválidas sem efeitos parciais.

#### Scenario: Preparação válida
- **WHEN** XBE sintético suportado é preparado
- **THEN** estado passa a prepared com CPU posicionada no entry point e sem executar instrução

### Requirement: Ownership isolado
Cada sessão SHALL possuir seus subsistemas e MUST NOT compartilhar memória guest mutável ou callbacks pendentes com outra sessão.

#### Scenario: Duas sessões
- **WHEN** duas sessões carregam programas diferentes
- **THEN** alterações e eventos de uma não afetam a outra

### Requirement: Step controlado
A sessão SHALL permitir steps sintéticos com orçamento, sincronizando ciclos CPU/scheduler e parando em halt, fault ou limite.

#### Scenario: Step até halt
- **WHEN** programa sintético preparado termina em HLT dentro do orçamento
- **THEN** sessão pausa e reporta estado/ciclos determinísticos

### Requirement: Diagnóstico pela ABI
A ABI C SHALL expor capacidades e resultado de preparação/estado sem fornecer ponteiros guest nem ação enganosa de executar jogos.

#### Scenario: UI consulta preparação
- **WHEN** shell consulta XBE sintético suportado
- **THEN** recebe estado e entry point serializáveis sem acesso direto à memória
