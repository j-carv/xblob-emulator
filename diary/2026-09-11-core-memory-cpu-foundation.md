# Diário de Engenharia: Fundação de Memória Guest, Scheduler e CPU IA-32 (2026-09-11)

## 1. Contexto e Objetivos

Nesta etapa correspondente à change OpenSpec `add-core-memory-cpu-foundation` (Marco 2), implementamos a infraestrutura inicial de emulação e computação do projeto **xblob**:
- `libs/memory` (`xblob_memory`): Gerenciador de RAM física de 64 MiB (varejo) e 128 MiB (devkit), espaço de endereçamento plano de 32 bits (`AddressSpace`), permissões estritas de acesso (`MemoryPermission`), despacho tipado de MMIO e garantia de atomicidade na detecção de sobreposições e limites de regiões.
- `libs/core` (`xblob_core`): Scheduler determinístico baseado em relógio virtual monotônico de 64 bits (`Cycle`), ordenação estável por `(ciclo, sequência)` com desempate determinístico, cancelamento por id opaco e execução com orçamentos de ciclo e eventos para prevenção de laços infinitos.
- `libs/cpu` (`xblob_cpu`): Fundação arquitetural do intérprete IA-32 (registradores gerais `EAX`..`EDI`, `EIP`, `EFLAGS` com invariante do bit 1 reservado fixo em 1, seletores de segmento mínimos e estados de ciclo de vida), decodificação/fetch transacional atômico, subconjunto inicial de instruções (`NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32`, `SUB EAX, imm32`, `JMP rel8`, `JMP rel32`), cálculo rigoroso de flags aritméticas (`CF`, `PF`, `AF`, `ZF`, `SF`, `OF`), tratamento de opcode inválido com preservação do `EIP` original e runner determinístico com orçamentos configuráveis.
- Governança e Arquitetura: Atualização de `AGENTS.md`, `README.md`, `ARCHITECTURE.md` e READMEs dos subsistemas para documentar a governança OpenSpec autônoma e a arquitetura da futura interface desktop (React + TypeScript, Tauri v2 shell, núcleo em C/C++20 com ABI C estável e Rust estritamente restrito a adaptador de FFI/IPC).

---

## 2. Decisões Técnicas Tomadas

1. **Separação Modular e Dependências Direcionais Estritas**:
   - `xblob_memory` depende exclusivamente de `xblob_common`.
   - `xblob_core` depende exclusivamente de `xblob_common`.
   - `xblob_cpu` depende de `xblob_common` e `xblob_memory`.
   - `xblob_core` e `xblob_cpu` são completamente desacoplados entre si; a coordenação temporal ocorre unicamente no harness de integração ou no orquestrador de máquina.
2. **Atomicidade de Memória e Prevenção de Mutações Parciais**:
   - Mapeamentos no `AddressSpace` verificam colisões e sobreposições antes de registrar qualquer região, revertendo alterações atomicamente em caso de erro.
   - Leituras e escritas de 8, 16 e 32 bits little-endian aceitam desalinhamentos desde que contidos na mesma região, mas acessos que cruzam fronteiras entre regiões são rejeitados antes de modificar o estado da RAM.
   - MMIO é estritamente tipado (larguras de 1, 2 e 4 bytes) e falha sem degradar silenciosamente para leitura/escrita em RAM.
   - Fetch de instruções exige permissão explícita `MemoryPermission::Execute`.
3. **Determinismo Temporal Absoluto**:
   - O `DeterministicScheduler` não emprega relógios reais (`steady_clock`), baseando-se unicamente em ciclos monotônicos de 64 bits.
   - Empates no mesmo ciclo alvo são desempatados deterministicamente pelo número monotônico de sequência de inserção.
   - Reagendamentos para o ciclo corrente são permitidos, mas um limite configurável de eventos (`max_events`) impede que cadeias infinitas de eventos congelem a execução.
4. **Semântica IA-32 e Conformidade de Flags**:
   - `EFLAGS` mantém permanentemente ativo o bit 1 reservado (`0x00000002`), inclusive em operações de reset e sobrescrita direta.
   - Cálculos aritméticos em `ADD` e `SUB` utilizam promoção para 64 bits e operações bitwise estritamente sem comportamento indefinido de overflow assinado em C++.
   - `JMP rel8` e `JMP rel32` calculam o destino relativo ao término da instrução com aritmética modular de 32 bits.
   - Opcodes inválidos ou truncados preservam o `EIP` original no endereço de início da instrução para diagnóstico preciso.
5. **Fixtures Exclusivamente Sintéticas**:
   - Nenhuma ROM proprietária, binário extraído do hardware oficial ou dados do Xbox SDK foram incluídos; todas as validações utilizam sequências sintéticas geradas programaticamente.

---

## 3. Comandos Executados e Resultados de Verificação

### 3.1 Ambiente Host
- Plataforma: macOS ARM64 (Apple Silicon)
- Compilador: AppleClang 21.0.0 (clang-2100.1.1.101)
- CMake: 4.4.3
- Clang-Format: 23.1.1

### 3.2 Compilação Padrão e Testes CTest
```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```
Resultado:
```text
Test project /Users/fl4k/Documents/projects/xblob/build/default
    Start 1: test_common
1/8 Test #1: test_common ......................   Passed    0.00 sec
    Start 2: test_io
2/8 Test #2: test_io ..........................   Passed    0.00 sec
    Start 3: test_formats
3/8 Test #3: test_formats .....................   Passed    0.01 sec
    Start 4: test_memory
4/8 Test #4: test_memory ......................   Passed    4.82 sec
    Start 5: test_scheduler
5/8 Test #5: test_scheduler ...................   Passed    0.01 sec
    Start 6: test_cpu
6/8 Test #6: test_cpu .........................   Passed    4.33 sec
    Start 7: test_inspector_integration
7/8 Test #7: test_inspector_integration .......   Passed    0.01 sec
    Start 8: test_machine_integration
8/8 Test #8: test_machine_integration .........   Passed    1.61 sec

100% tests passed out of 8
Total Test time (real) = 10.79 sec
```

### 3.3 Verificação com AddressSanitizer e UndefinedBehaviorSanitizer
```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure
```
Resultado:
```text
Test project /Users/fl4k/Documents/projects/xblob/build/asan-ubsan
    Start 1: test_common
1/8 Test #1: test_common ......................   Passed    0.65 sec
    Start 2: test_io
2/8 Test #2: test_io ..........................   Passed    0.50 sec
    Start 3: test_formats
3/8 Test #3: test_formats .....................   Passed    0.49 sec
    Start 4: test_memory
4/8 Test #4: test_memory ......................   Passed    9.03 sec
    Start 5: test_scheduler
5/8 Test #5: test_scheduler ...................   Passed    0.53 sec
    Start 6: test_cpu
6/8 Test #6: test_cpu .........................   Passed    7.62 sec
    Start 7: test_inspector_integration
7/8 Test #7: test_inspector_integration .......   Passed    0.66 sec
    Start 8: test_machine_integration
8/8 Test #8: test_machine_integration .........   Passed    3.37 sec

100% tests passed out of 8
Total Test time (real) = 22.85 sec
```
Nenhum vazamento de memória, acesso fora de limites ou comportamento indefinido foi detectado em nenhum teste.

### 3.4 Verificação de Estilo de Código (Clang-Format)
```bash
./tools/check-format.sh
```
Resultado:
```text
==> Verificando formato de código C/C++...
==> Todos os arquivos C/C++ verificados estão em conformidade com o padrão.
```

### 3.5 Verificação Estática via clangd
```bash
clangd --check=libs/memory/src/address_space.cpp --compile-commands-dir=build/default
clangd --check=libs/core/src/scheduler.cpp --compile-commands-dir=build/default
clangd --check=libs/cpu/src/cpu.cpp --compile-commands-dir=build/default
clangd --check=libs/cpu/src/registers.cpp --compile-commands-dir=build/default
clangd --check=libs/memory/src/ram.cpp --compile-commands-dir=build/default
```
Resultado: Todas as unidades compilaram com 0 diagnósticos ou erros (`All checks completed, 0 errors`).

### 3.6 Teste de Integração Sintética Determinística
O teste `test_machine_integration` instanciou `Ram`, `AddressSpace`, `DeterministicScheduler` e `Cpu`, executando um programa sintético duas vezes consecutivas em isolamento total. Todas as asserções de igualdade estrita foram satisfeitas:
- `instructions_executed`: 6
- `final_cycle`: 6
- `final_context.eip`: 0x1016
- `final_context.EAX`: 115 (100 + 25 - 10)
- `final_context.EBX`: 50
- `event_log`: [102, 104, 105] na mesma ordem de despacho
- `memory_word` em 0x2000: 0xCAFEBABE

---

## 4. Limitações Conhecidas

- **Escopo do Marco 2**: O emulador não executa binários XBE completos, não carrega o kernel `xboxkrnl.exe`, não emula o hardware gráfico NV2A nem periféricos de áudio/USB. A CPU suporta exclusivamente o subconjunto inicial de instruções sintéticas (`NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32`, `SUB EAX, imm32`, `JMP rel8`, `JMP rel32`).
- **Validação de Plataformas Linux e Windows**: A compatibilidade nessas plataformas é coberta pelo pipeline automatizado de CI em `.github/workflows/ci.yml`.

---

## 5. Próximos Passos (Marco 3)

1. Implementação de tabelas de páginas de hardware de 4 KB e emulação de MMU do x86.
2. Modelagem do barramento PCI host-to-PCI e alocação de barramentos de configuração.
3. Carregador inicial de seções XBE para memória virtual guest e preparação para chamadas de sistema HLE.
