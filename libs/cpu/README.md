# Emulação de CPU IA-32 (libs/cpu)

Este subsistema provê a fundação do intérprete de instruções IA-32 de 32 bits para a arquitetura x86.

## Estado Atual (Marco: Expansão de Execução de Títulos IA-32)
- Estado arquitetural IA-32: registradores gerais (`EAX`, `ECX`, `EDX`, `EBX`, `ESP`, `EBP`, `ESI`, `EDI`), acessos em 8/16/32 bits (`GetGpr8`/`SetGpr8`, `GetGpr16`/`SetGpr16`), ponteiro de instrução `EIP`, `EFLAGS` com invariante do bit 1 reservado forçado em 1 (`0x00000002` no reset), seletores de segmento (`CS`, `DS`, `SS`, `ES`, `FS`, `GS`), registradores de controle e descritores (`CR0`, `CR2`, `CR3`, `IDTR`), e ciclo de vida (`Running`, `Halted`, `Faulted`).
- Pipeline transacional de decodificação e execução com rollback integral de registradores e memória em falhas (#DE, #UD, #PF, #GP), garantindo ausência de mutação de estado em exceções.
- Suporte a prefixos e diagnóstico de formas não suportadas:
  - Prefixos de repetição (`REP` 0xF3, `REPNE` 0xF2), overrides de segmento (`CS`, `DS`, `SS`, `ES`, `FS`, `GS`), override de tamanho de operando/endereço (`0x66`, `0x67`) e `LOCK` (0xF0).
  - Detecção de conflitos de prefixos (`0xF2` + `0xF3`) e prefixos em instruções inválidas.
  - Metadados diagnósticos estruturados (`UnsupportedFormInfo`) contendo `fault_eip`, `opcode_bytes` e `reason` para diferenciar formas não suportadas de opcodes inválidos (#UD).
- Instruções suportadas:
  - **Controle e Movimentação Básica**: `NOP`, `HLT`, `MOV` (todas as variantes fundamentais reg/mem/imm/moffs), `LEA`, `PUSH`, `POP`, `PUSHFD`, `POPFD`, `CALL` (rel32, r/m32), `RET` (near, imm16), `JMP` (rel8, rel32, r/m32), `Jcc` (rel8, rel32), `INT`, `IRET`, `CLI`, `STI`.
  - **Aritmética e Lógica Inteira**: `ADD`, `ADC`, `SUB`, `SBB`, `CMP`, `AND`, `OR`, `XOR`, `TEST`, `INC`, `DEC`.
  - **Deslocamentos e Rotações**: `SHL`/`SAL`, `SHR`, `SAR`, `ROL`, `ROR` nos modos count 1, imm8 e registrador CL (com máscara `count & 0x1F`, preservação estrita de flags em contagem zero e flags aritméticas precisas).
  - **Multiplicação e Divisão**:
    - `MUL` (EDX:EAX e AX com flags `CF`/`OF`),
    - `IMUL` (formas de 1 operando EDX:EAX, 2 operandos reg-r/m, e 3 operandos reg-r/m-imm32/imm8),
    - `DIV` e `IDIV` 32-bit com geração estrita de `#DE` (vetor 0) para divisão por zero e overflow de quociente, sem mutação parcial de registradores e imune a UB no host (`INT64_MIN / -1`).
  - **Manipulação de Bits e Extensão**: `MOVZX` (byte/word para dword), `MOVSX` (byte/word com extensão de sinal para dword), `CDQ` (extensão de EAX para EDX:EAX), `BT` (bit test com imediato ou registrador atualizando `CF`), `SETcc` (todas as 16 condições de byte).
  - **Operações de String**: `MOVSB`/`MOVSD`, `STOSB`/`STOSD`, `LODSB`/`LODSD`, `CMPSB`/`CMPSD`, `SCASB`/`SCASD`, `CLD`, `STD` respeitando `DF` (incremento ou decremento por 1 ou 4 bytes).
  - **Micro-steps de Repetição Delimitados e Retomáveis**: `REP`/`REPE`/`REPNE` implementado em micro-steps determinísticos respeitando o orçamento (budget) de instruções e ciclos. Interrupções deixam o `EIP` apontando para o prefixo `REP` com `ECX`, `ESI` e `EDI` contendo o progresso intermediário exato para retomada determinística.
  - **Operações Atômicas**: `XCHG` (reg-reg e reg-mem) e `CMPXCHG` (byte e dword) no modelo single-core, com escrita atômica sob igualdade e atualização de acumulador com flags `ZF` sob desigualdade.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/memory`.
- Proibido depender de `libs/core`.
- Tamanho modular de arquivo restrito a < 500 linhas por arquivo.
