# Emulação de CPU IA-32 (libs/cpu)

Este subsistema provê a fundação do intérprete de instruções IA-32 de 32 bits para a arquitetura x86.

## Estado Atual (Marco 2: Ativo)
- Estado arquitetural IA-32: registradores gerais (`EAX`, `ECX`, `EDX`, `EBX`, `ESP`, `EBP`, `ESI`, `EDI`), ponteiro de instrução `EIP`, `EFLAGS` com invariante do bit 1 reservado forçado em 1 (`0x00000002` no reset), seletores de segmento mínimos (`CS`, `DS`, `SS`, `ES`, `FS`, `GS`) e ciclo de vida (`Running`, `Halted`, `Faulted`).
- Fetch transacional via interface de memória (`AddressSpace`), rejeitando acessos a regiões não executáveis e garantindo ausência de mutação parcial em instruções truncadas.
- Decodificação e execução do subconjunto inicial:
  - `NOP` (0x90)
  - `HLT` (0xF4)
  - `MOV r32, imm32` (0xB8..0xBF)
  - `ADD EAX, imm32` (0x05) e `SUB EAX, imm32` (0x2D) com cálculo rigoroso de `CF`, `PF`, `AF`, `ZF`, `SF` e `OF` sem overflow assinado indefinido.
  - `JMP rel8` (0xEB) e `JMP rel32` (0xE9) com aritmética de deslocamento com sinal e wrap modular de 32 bits.
- Tratamento de opcode inválido com preservação do `EIP` original para fins de diagnóstico.
- Tabela de custos determinísticos por instrução e runner com orçamentos configuráveis de instruções e ciclos.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/memory`.
- Proibido depender de `libs/core`.
