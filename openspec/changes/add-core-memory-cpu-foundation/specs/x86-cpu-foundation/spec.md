## Purpose

Estabelece um intérprete IA-32 de referência pequeno e verificável, destinado a programas sintéticos e à expansão incremental por conformidade arquitetural.

## ADDED Requirements

### Requirement: Estado básico IA-32
A CPU SHALL modelar `EAX`, `ECX`, `EDX`, `EBX`, `ESP`, `EBP`, `ESI`, `EDI`, `EIP`, `EFLAGS` e seletores de segmento mínimos com valores de reset documentados e tipos de largura fixa.

#### Scenario: Reset da CPU
- **WHEN** a CPU é reinicializada
- **THEN** todos os registradores expõem exatamente os valores de reset documentados e o estado é `running`

### Requirement: Fetch seguro de instruções
A CPU SHALL buscar bytes exclusivamente pela interface de memória com permissão de execução e MUST propagar falhas de endereço ou proteção sem acessar memória do host diretamente.

#### Scenario: Opcode em página não executável
- **WHEN** `EIP` aponta para memória legível mas não executável
- **THEN** o step termina em estado `faulted` com falha de proteção e sem executar a instrução

### Requirement: Subconjunto inicial de instruções
O intérprete SHALL decodificar e executar `NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32`, `SUB EAX, imm32`, `JMP rel8` e `JMP rel32`, atualizando `EIP` pela largura exata da instrução ou pelo destino relativo.

#### Scenario: Programa aritmético sintético
- **WHEN** a CPU executa uma sequência válida de `MOV EAX`, `ADD EAX`, `SUB EAX` e `HLT`
- **THEN** `EAX`, `EIP`, flags e estado halted correspondem à semântica IA-32 documentada

#### Scenario: Salto relativo negativo
- **WHEN** um salto relativo curto possui deslocamento negativo válido
- **THEN** o destino é calculado relativamente ao endereço após a instrução com aritmética de 32 bits

### Requirement: Flags aritméticas essenciais
`ADD EAX, imm32` e `SUB EAX, imm32` SHALL atualizar pelo menos `CF`, `PF`, `AF`, `ZF`, `SF` e `OF` conforme semântica IA-32 de 32 bits, preservando bits de `EFLAGS` não afetados.

#### Scenario: Adição com overflow assinado
- **WHEN** `0x7fffffff` recebe adição de `1`
- **THEN** o resultado é `0x80000000`, `OF` e `SF` ficam ativos, e `CF` permanece inativo

#### Scenario: Subtração com borrow
- **WHEN** zero recebe subtração de um
- **THEN** o resultado é `0xffffffff`, `CF` e `SF` ficam ativos, e `ZF` permanece inativo

### Requirement: Opcode inválido e instrução truncada
A CPU SHALL distinguir opcode não implementado de instrução truncada, transicionar para `faulted` e preservar o endereço da instrução que falhou para diagnóstico.

#### Scenario: Opcode desconhecido
- **WHEN** a CPU encontra um opcode fora do subconjunto suportado
- **THEN** retorna falha `invalid opcode`, mantém o endereço inicial para diagnóstico e não modifica registradores não relacionados

#### Scenario: Imediato truncado
- **WHEN** um `MOV r32, imm32` não possui todos os bytes do imediato em região executável
- **THEN** a CPU retorna falha de fetch truncado sem aplicar parcialmente a instrução

### Requirement: Execução com orçamento de ciclos
Cada step SHALL reportar uma quantidade determinística e documentada de ciclos, e a execução contínua SHALL respeitar limite de instruções ou ciclos fornecido pelo consumidor.

#### Scenario: Orçamento esgotado
- **WHEN** um programa não alcança `HLT` antes do orçamento configurado
- **THEN** o controle retorna sem falha, informando orçamento esgotado e estado completo retomável
