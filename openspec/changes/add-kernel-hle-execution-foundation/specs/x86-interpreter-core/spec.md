## Purpose

Amplia o intérprete de referência IA-32 com operandos e instruções suficientes para programas diagnósticos sintéticos, mantendo comportamento determinístico e faults sem mutação parcial.

## ADDED Requirements

### Requirement: Decodificação defensiva
O intérprete SHALL decodificar prefixos suportados, opcode, ModR/M, SIB, displacement e immediate byte a byte, limitando instruções a 15 bytes e rejeitando formas não suportadas antes de efeitos arquiteturais.

#### Scenario: Instrução truncada
- **WHEN** faltam bytes de displacement ou immediate
- **THEN** EIP, registradores, flags e memória permanecem inalterados e um fault estruturado é retornado

### Requirement: Operandos de 32 bits
O sistema SHALL suportar registrador e memória em addressing 32-bit para ModR/M/SIB, incluindo base, index, scale e displacement, com acesso virtual protegido.

#### Scenario: Operando SIB
- **WHEN** uma instrução referencia `[base + index*scale + disp]`
- **THEN** o endereço efetivo é calculado com wrap arquitetural de 32 bits e acessado pela MMU

### Requirement: Conjunto fundamental
O intérprete SHALL implementar formas 32-bit necessárias de MOV, LEA, PUSH, POP, PUSHF, POPF, CALL, RET, ADD, ADC, SUB, SBB, INC, DEC, CMP, TEST, AND, OR, XOR e branches condicionais básicos, além do conjunto existente.

#### Scenario: Call e retorno
- **WHEN** CALL relativo seguido de RET executa com pilha válida
- **THEN** endereço de retorno é armazenado little-endian e EIP retorna ao chamador

### Requirement: Flags arquiteturais
Operações SHALL atualizar CF, PF, AF, ZF, SF e OF conforme IA-32 para a largura executada, preservando bits não afetados.

#### Scenario: Overflow signed
- **WHEN** ADD de dois valores positivos gera resultado negativo
- **THEN** OF é setado e CF reflete carry unsigned independentemente

### Requirement: Regressão diferencial
Cada opcode implementado SHALL ter vetores de borda e, quando possível sem dependência proprietária, comparação contra modelo de referência aberto/documentado.

#### Scenario: Vetores de limite
- **WHEN** testes exercitam zero, sinal, carry e overflow
- **THEN** registradores, flags, EIP, memória e ciclos coincidem com resultados esperados documentados
