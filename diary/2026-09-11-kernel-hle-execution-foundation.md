# Diário de Bordo: Fundação de Execução IA-32 e Kernel HLE Clean-Room

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação integral da change OpenSpec `add-kernel-hle-execution-foundation` (37 tarefas).

---

## 1. Visão Geral e Objetivos do Marco

Este marco introduziu a capacidade de execução controlada e determinística de programas diagnósticos sintéticos no emulador **xblob**, cobrindo:
1. **Decoder e Executor IA-32 Modular (`libs/cpu`)**:
   - Pipeline de 3 fases estritas: `Decode` -> `Validate` -> `Commit` com garantia transacional.
   - Decodificador de ModR/M e SIB 32-bit completo, displacement e immediate checked, limite arquitetural de 15 bytes.
   - Abstração de operandos puramente virtual (`Register`, `Immediate`, `Memory` por endereço de convidado, sem vazamento de ponteiros host).
   - Famílias de instruções implementadas:
     - Dados: `MOV`, `LEA`.
     - Stack: `PUSH`, `POP`, `PUSHF`, `POPF`, `CALL`, `RET`.
     - ALU: `ADD`, `ADC`, `SUB`, `SBB`, `INC`, `DEC`, `CMP`, `AND`, `OR`, `XOR`, `TEST` com cálculo puro e exato de flags (`CF`, `PF`, `AF`, `ZF`, `SF`, `OF`).
     - Branch: `JZ`/`JE`, `JNZ`/`JNE`, `JC`/`JB`, `JNC`/`JAE`, `JS`, `JNS`, `JO`, `JNO` com saltos `rel8` e `rel32`.
     - Sistema/Controle: `NOP`, `HLT`, `CLI`, `STI`, `INT`, `IRET`.
2. **Exceções Arquiteturais e Interrupções (`libs/cpu`)**:
   - Separação categórica entre exceções arquiteturais da CPU (`#UD` vetor 6, `#GP` vetor 13, `#PF` vetor 14) e erros internos do host.
   - Suporte a IDTR e tabela IDT com Interrupt e Trap Gates de 32 bits, validação de limite, bit de presença e DPL.
   - Criação atômica de stack frame same-ring e retorno sem privilégios com `IRET`.
   - Flag de interrupção (`IF`), fila de IRQs determinística e entrega estritamente em fronteiras de instrução.
3. **Kernel HLE Clean-Room (`libs/kernel`)**:
   - Biblioteca isolada em conformidade com as regras de clean-room, documentada a partir de especificações públicas.
   - Tabela de ordinais com registro controlado (`ExportRegistry`), proibição de duplicatas e proteção contra chamadas não suportadas.
   - Despachante de thunks sintéticos com leitura/escrita checked de argumentos guest e retorno ao guest via `EAX`.
   - Coletor de depuração (`BufferedDebugSink`) com buffer limitado, sanitização UTF-8 e prevenção de alocação desenfreada.
   - Heap guest determinístico (`GuestHeap`) com alinhamento configurável, split e coalesce de blocos livres, e detecção de double-free / corrupção de ponteiros.
   - Escalonador cooperativo de threads (`ThreadScheduler`) com IDs geracionais, chaveamento determinístico de contexto e isolamento de stacks.
   - Objetos de sincronização mínimos (`EventObject`, `MutexObject` recursivo) com filas FIFO reproduzíveis.
4. **Sessão, Tracing e Fixtures (`libs/machine`, `tests/`)**:
   - Buffer circular limitado de eventos de rastreamento (`TraceRingBuffer`) registrando instruções, exceções, HLE e troca de contexto com contador de eventos descartados.
   - Fixtures sintéticas de integração exercitando determinismo estrito: execuções repetidas da mesma máquina produzem estados, ciclos, saídas e rastros idênticos byte a byte.
5. **ABI C 1.2 Retrocompatível e Interface Desktop (`libs/c_api`, `apps/desktop`)**:
   - Evolução da ABI C para a versão 1.2 com capability `XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION` e funções com verificação de tamanho de estrutura (`struct_size`).
   - Adaptação Rust FFI com RAII estrito e sem vazamento de ponteiros ou dependências proprietárias.
   - Interface desktop React com executor diagnóstico dedicado, orçamentos obrigatórios de ciclo/instrução, cancelamento e visualização de telemetria de trace.

---

## 2. Limitações Técnicas Honestas

> [!WARNING]
> **Limitações Atuais do Projeto**:
> - **Incompatibilidade com Mídia Comercial**: O emulador continua deliberadamente incapaz de executar jogos comerciais ou executáveis não sintéticos. Mídias arbitrárias permanecem estritamente restritas à inspeção estrutural.
> - **Subconjunto de Instruções IA-32**: Apenas as formas de instruções especificadas para a fundação diagnóstica foram implementadas. Instruções SSE, MMX, FPU x87 e modos de 16 bits não são suportados e disparam exceção `#UD`.
> - **Kernel HLE Parcial**: Apenas serviços essenciais da fundação (como `DbgPrint`, heap, threads cooperativas e eventos/mutexes) foram modelados sinteticamente. O restante do subsistema de drivers e chamadas de sistema permanece não implementado.
> - **Multithreading Cooperativo**: As threads de convidado operam de modo cooperativo através de pontos de rendição (`Yield`/`Wait`/`Exit`), sem temporizadores preemptivos de APIC host.

---

## 3. Comandos e Verificações Executados

1. **Configuração e Compilação Normal**:
   ```bash
   cmake -B build -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   cmake --build build -j8
   ctest --test-dir build --output-on-failure
   # Resultado: 16/16 testes aprovados (100%)
   ```
2. **Configuração e Compilação com Sanitizers (ASan + UBSan)**:
   ```bash
   cmake -B build-san -S . -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DXBLOB_ENABLE_SANITIZERS=ON
   cmake --build build-san -j8
   ctest --test-dir build-san --output-on-failure
   # Resultado: 16/16 testes aprovados com zero leaks e zero undefined behavior (100%)
   ```
3. **Formatação de Código e Diagnóstico Estático**:
   ```bash
   clang-format --dry-run --Werror $(find libs tests apps/inspect -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h')
   clangd --check=libs/cpu/src/decoder.cpp --compile-commands-dir=build
   # Resultado: Formatação 100% conforme, zero avisos e zero erros de compilação
   ```
4. **Backend Rust Desktop**:
   ```bash
   cargo fmt --check
   cargo clippy -- -D warnings
   cargo test --locked
   # Resultado: 5/5 testes aprovados, clippy e formatação limpos
   ```
5. **Frontend Web Desktop**:
   ```bash
   npm run lint
   npm run typecheck
   npm test
   npm run build
   # Resultado: 25/25 testes aprovados, compilação de produção bem-sucedida
   ```

---

## 4. Próximos Passos Recomendados

1. Implementação de instruções adicionais de rotação/deslocamento (`SHL`, `SHR`, `SAR`, `ROL`, `ROR`) e instruções de movimentação de strings (`MOVS`, `STOS`, `CMPS`).
2. Expansão dos serviços de Kernel HLE para abranger gerenciamento básico de E/S de arquivos virtuais (`NtOpenFile`, `NtReadFile`).
3. Modelagem sintética de dispositivos PCI básicos no barramento convidado.
