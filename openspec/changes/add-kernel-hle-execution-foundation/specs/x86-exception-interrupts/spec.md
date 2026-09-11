## Purpose

Define faults, traps e interrupções iniciais suficientes para execução protegida de 32 bits e integração determinística com serviços HLE.

## ADDED Requirements

### Requirement: Exceções arquiteturais
A CPU SHALL distinguir ao menos #UD, #GP e #PF, preservando endereço de fault e código de erro quando aplicável.

#### Scenario: Opcode inválido
- **WHEN** opcode desconhecido é executado
- **THEN** a CPU produz #UD no EIP da instrução sem tratá-lo como erro genérico do host

### Requirement: IDT de 32 bits
O sistema SHALL ler IDTR e gates interrupt/trap de 32 bits por memória guest, validar limite, present, tipo e privilégio e construir frame na pilha guest atomicamente.

#### Scenario: Gate válido
- **WHEN** interrupção aponta para gate presente e pilha gravável
- **THEN** EFLAGS, CS e EIP são empilhados e controle passa ao handler

#### Scenario: Falha durante frame
- **WHEN** qualquer escrita do frame falha
- **THEN** nenhuma escrita parcial ou alteração de contexto permanece

### Requirement: IF e fila determinística
Interrupções mascaráveis SHALL respeitar IF e ser entregues em fronteira de instrução por ordem estável de ciclo e sequência; NMI e prioridades fora do escopo MUST ser explícitas.

#### Scenario: IF limpo
- **WHEN** IRQ está pendente com IF=0
- **THEN** permanece pendente sem interromper a instrução atual

### Requirement: Retorno de interrupção
IRET 32-bit SHALL restaurar EIP, CS e EFLAGS validados de frame suportado, rejeitando mudanças de privilégio ainda não implementadas.

#### Scenario: IRET same-ring
- **WHEN** handler retorna no mesmo ring
- **THEN** contexto anterior é restaurado deterministicamente
