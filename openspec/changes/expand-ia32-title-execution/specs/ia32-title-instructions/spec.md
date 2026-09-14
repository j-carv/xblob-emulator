## Purpose
Amplia IA-32 interpretado para padrões comuns de código compilado, mantendo atomicidade.

## ADDED Requirements

### Requirement: Shifts e rotates
CPU SHALL implementar SHL/SAL, SHR, SAR, ROL e ROR nas formas register/memory com count 1, imm8 e CL, respeitando masking e flags definidos.
#### Scenario: Count zero
- **WHEN** count efetivo é zero
- **THEN** destino e flags permanecem inalterados

### Requirement: Multiply e divide
CPU SHALL implementar MUL, IMUL e DIV/IDIV 32-bit selecionados com resultados EDX:EAX e #DE para zero/overflow.
#### Scenario: Divide overflow
- **WHEN** quociente não cabe
- **THEN** contexto permanece atômico e #DE é produzido

### Requirement: Extensão e bits
CPU SHALL implementar MOVZX, MOVSX, CDQ, BT e forms essenciais de SETcc.
#### Scenario: MOVSX
- **WHEN** byte negativo é estendido
- **THEN** registrador recebe extensão de sinal correta sem alterar flags

### Requirement: Strings
CPU SHALL implementar MOVS/STOS/LODS/CMPS/SCAS byte/dword com DF e REP/REPE/REPNE bounded por budget.
#### Scenario: REP interrompível
- **WHEN** budget termina durante REP
- **THEN** EIP/ECX/ESI/EDI refletem progresso retomável deterministicamente

### Requirement: Atomicidade e diagnóstico
XCHG e CMPXCHG básicos SHALL ser atômicos no modelo single-core; forms desconhecidas retornam diagnóstico com opcode bytes, EIP e razão.
#### Scenario: Form unsupported
- **WHEN** opcode conhecido usa prefixo/form não implementada
- **THEN** stop distingue unsupported-form de invalid opcode
