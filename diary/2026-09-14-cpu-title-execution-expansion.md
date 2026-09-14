# Diário de Engenharia: Expansão de Execução de Títulos IA-32 na CPU (2026-09-14)

## 1. Contexto e Objetivos

Nesta etapa correspondente à child change OpenSpec `expand-ia32-title-execution` da lane exclusiva CPU (`libs/cpu`), expandimos a capacidade de decodificação e execução de instruções IA-32 para suportar os padrões de compilação essenciais de títulos Xbox (MSVC 2002/2003) sem introduzir regressões na suite existente nem violar o isolamento de componentes:
- Suporte abrangente a prefixos (`REP` 0xF3, `REPNE` 0xF2, `LOCK` 0xF0, overrides de segmento `CS`..`GS`, e diagnósticos para `0x66`/`0x67`).
- Mecanismo estruturado de diagnóstico `UnsupportedFormInfo` registrando `fault_eip`, bytes originais do stream e motivo semântico para distinguir formas de opcodes válidos ainda não suportadas de opcodes realmente inválidos (#UD / `ErrorCode::InvalidOpcode`).
- Deslocamentos e rotações (`SHL`/`SAL`, `SHR`, `SAR`, `ROL`, `ROR`) para tamanhos 8 e 32 bits, com contagens 1, imediato e registrador `CL`, obedecendo à máscara de 5 bits (`count & 0x1F`) e garantindo preservação rigorosa de todas as flags quando a contagem mascarada for 0.
- Multiplicação inteira (`MUL` sem sinal e `IMUL` com sinal em 1, 2 e 3 operandos) gerando produtos em `EDX:EAX` e configurando `CF`/`OF` estritamente conforme a semântica da arquitetura.
- Divisão inteira (`DIV` e `IDIV` 32-bit com dividendo de 64 bits em `EDX:EAX`) com verificação preventiva de divisor zero e overflow de quociente gerando Exceção de Divisão (#DE, vetor 0), garantindo ausência de comportamento indefinido (UB) no host (especialmente `INT64_MIN / -1`) e atomicidade completa (zero mutação parcial de `EAX`, `EDX` ou `EFLAGS` em caso de falha).
- Extensões e testes de bits: `MOVZX`, `MOVSX` (byte/word para dword sem tocar em flags), `CDQ` (extensão de `EAX` para `EDX:EAX`), `BT` (bit test registrador/imediato atualizando `CF`) e a família completa `SETcc` em operandos de 8 bits.
- Operações de string com Direction Flag (`DF`): `MOVSB`/`MOVSD`, `STOSB`/`STOSD`, `LODSB`/`LODSD`, `CMPSB`/`CMPSD`, `SCASB`/`SCASD`, `CLD` e `STD`.
- Micro-steps de repetição (`REP`, `REPE`, `REPNE`) delimitados e retomáveis: cada iteração consome 1 instrução do orçamento (budget) do runner, mantendo `EIP` apontando para a instrução REP enquanto houver iterações pendentes (`ECX > 0`), permitindo interrupções determinísticas por expiração de orçamento e retomada perfeita sem mutação espúria.
- Primitivas atômicas no modelo single-core: `XCHG` (reg-reg e reg-mem) e `CMPXCHG` (byte e dword) com pipeline transacional e rollback em caso de falha de acesso à memória.

---

## 2. Decisões de Arquitetura e Engenharia

1. **Modularidade e Limite Estrito de 500 Linhas por Arquivo**:
   - Para manter alta manutenibilidade e cumprir as regras do projeto, a implementação da CPU foi decomposta em módulos focados:
     - `libs/cpu/src/instruction_stream.hpp`: cursor seguro de fetch de bytes/imediatos sem uso de ponteiros host diretos.
     - `libs/cpu/src/decoder_data.hpp`: decodificação de movimentação de dados e pilha (`MOV`, `LEA`, `PUSH`, `POP`, `NOP`, `HLT`).
     - `libs/cpu/src/decoder_alu.hpp`: decodificação de operações aritméticas, lógicas e Grupo 1 / Grupo 3 (`ADD`, `SUB`, `CMP`, `TEST`, etc.).
     - `libs/cpu/src/decoder_shift.hpp`: decodificação do Grupo 2 (`SHL`, `SHR`, `SAR`, `ROL`, `ROR`).
     - `libs/cpu/src/decoder_mul_div.hpp`: decodificação de `IMUL` 2 e 3 operandos.
     - `libs/cpu/src/decoder_bit_ext.hpp`: decodificação de `MOVZX`, `MOVSX`, `CDQ`, `BT` e `SETcc`.
     - `libs/cpu/src/decoder_string.hpp`: decodificação de instruções de string e controle de flags `DF`.
     - `libs/cpu/src/decoder_atomic.hpp`: decodificação de `XCHG` e `CMPXCHG`.
     - `libs/cpu/src/decoder.cpp`: orquestrador de loop de prefixos e despacho.
     - `libs/cpu/src/executor_shift.cpp`: execução pura de shifts/rotates.
     - `libs/cpu/src/executor_muldiv.cpp`: execução pura de multiplicação/divisão e tratamento #DE.
     - `libs/cpu/src/executor_bit_ext.cpp`: execução de extensões, bit tests e avaliações de condição `SETcc`.
     - `libs/cpu/src/executor_string.cpp`: execução de strings e micro-stepping de prefixos `REP`.
     - `libs/cpu/src/executor_atomic.cpp`: execução atômica single-core.
   - Todos os arquivos do subsistema possuem estritamente menos de 500 linhas de código.

2. **Isolamento de Memória Guest e Zero Host Pointers**:
   - Todo acesso à memória do guest opera exclusivamente por meio de endereços virtuais (`GuestAddr`) e chamadas de métodos de `AddressSpace` (`Read8`, `Read16`, `Read32`, `Write8`, `Write16`, `Write32`). Nenhum ponteiro direto de memória do host é vazado ou manipulado na lógica do interpretador.

3. **Pipeline Transacional e Atomicidade de Exceções**:
   - Instruções que envolvem leitura e escrita em memória (como `CMPXCHG` ou instruções de string) efetuam as leituras necessárias antes de efetivar mutações no estado dos registradores ou da memória.
   - Em caso de falha de decodificação, violação de página ou exceção de divisão (#DE), o `CpuContext` permanece inalterado e o `EIP` preserva o endereço inicial da instrução causadora.

4. **Execução de REP Retomável via Micro-steps**:
   - Instruções com prefixo `REP`/`REPE`/`REPNE` executam uma única iteração de micro-step por ciclo do runner quando há elementos pendentes. Ao final do micro-step, caso `ECX > 0` e as condições de terminação (`ZF`) não tenham sido satisfeitas, `StepOutcome` reporta um salto para o próprio `EIP` da instrução REP.
   - Caso o orçamento do runner se esgote no meio da sequência, o estado da CPU (`EIP`, `ECX`, `ESI`, `EDI`) permanece perfeitamente consistente, permitindo invocar novamente `RunWithBudget` para continuar exatamente de onde parou.

---

## 3. Verificação e Testes

### 3.1 Suite de Testes CPU
- `tests/unit/test_cpu.cpp`: 6 testes fundamentais passando integralmente.
- `tests/unit/test_cpu_extended.cpp`: 17 testes abrangendo decodificação ModR/M/SIB, ALU e flags, pilha e saltos, IDT/IRET, limites de instrução, conflitos de prefixos, shifts e rotates em todos os modos, multiplicação e divisão com #DE e prevenção de UB, extensões e SETcc, strings com DF=0 e DF=1, micro-stepping e retomada de REP com interrupção por budget, e operações atômicas com verificação de falha de memória.

### 3.2 Execução de Testes Normais e Sanitizers
- Compilação limpa em Debug padrão e execução CTest:
  `ctest --test-dir build/default -R cpu --output-on-failure`: 2/2 testes passaram (100%).
- Compilação limpa no preset `asan-ubsan` (AddressSanitizer + UndefinedBehaviorSanitizer):
  `ctest --preset asan-ubsan -R cpu --output-on-failure`: 2/2 testes passaram (100%), com zero vazamentos ou comportamentos indefinidos.
- Formatação de código com `clang-format` aplicada a todos os arquivos alterados e criados.

---

## 4. Governança OpenSpec

- A change `expand-ia32-title-execution` foi integralmente implementada e verificada.
- Ownership estritamente respeitado: alterações limitadas a `libs/cpu/**`, `tests/unit/test_cpu*`, `diary/*cpu*`, e `openspec/changes/expand-ia32-title-execution/**`.
- Nenhum arquivo fora do escopo atribuído foi modificado.
