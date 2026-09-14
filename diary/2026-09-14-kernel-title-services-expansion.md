# Diário de Bordo: Expansão de Serviços de Título e Kernel HLE Clean-Room

**Data**: 14 de setembro de 2026  
**Contexto**: Implementação integral da child change OpenSpec `expand-kernel-title-services` (14 tarefas) na lane exclusiva `kernel-services`.

---

## 1. Visão Geral e Arquitetura da Solução

Neste marco da lane Kernel, expandiu-se a camada de emulação de alto nível (HLE) para suportar os serviços de sistema fundamentais requeridos por títulos e binários convidados, preservando estritamente:
1. **Conformidade Clean-Room**:
   - Projeto desenvolvido exclusivamente com base em documentação pública, comportamento observado de interfaces NT/Xbox e especificações abertas de arquitetura.
   - Nenhuma linha ou estrutura derivada de SDKs proprietários ou vazamentos confidenciais.
2. **Determinismo Estrito de Tempo e Relógios**:
   - `KernelTime` modelado a partir da frequência da CPU (`733.333.333 Hz`).
   - `KeQueryPerformanceCounter`, `KeQueryPerformanceFrequency`, `KeQuerySystemTime`, `KeQueryInterruptTime` e `KeTickCount` são funções puras dos ciclos acumulados da CPU emulada, sem qualquer dependência ou deriva de relógios de parede do sistema operacional host.
   - `TimerObject` com suporte a `NotificationTimer` e `SynchronizationTimer`, modos one-shot e periódicos com avanço determinístico de múltiplos períodos e cancelamento livre de eventos órfãos (`KeCancelTimer`).
3. **Memória Virtual e Heap Guest Robusto**:
   - `VirtualMemoryManager` com controle de estados (`MEM_RESERVE`, `MEM_COMMIT`, `MEM_DECOMMIT`, `MEM_RELEASE`), consulta de regiões (`MemoryBasicInformation32`) e alteração de proteção de páginas.
   - Validações de overflow, faixas fora de limite e rollback transacional caso haja sobreposição indevida.
   - `GuestHeap` e `GuestHeapManager` com alocação alinhada (`AllocateAligned`), realocação (`ReAllocate`), medição de bloco (`GetBlockSize`), prevenção de double-free, coalescência de blocos livres e heaps secundários.
4. **Sincronização e Handles Generacionais**:
   - `HandleTable` com validação estrita de índice e geração, impedindo que descritores fechados ou reutilizados causem acessos a objetos obsoletos (stale handles).
   - `SemaphoreObject` com limite máximo e fila FIFO de liberação.
   - `MutexObject` com validação de thread proprietária e contagem de recursão reentrante.
   - `WaitForSingleObject` e `WaitForMultipleObjects` com validação atômica preliminar de todos os handles convidados, suporte a `WaitAny` e `WaitAll`, registro em filas de espera e prazos de timeout mapeados para ciclos da CPU.
5. **I/O Síncrono Prioritário e Assincronismo Honesto**:
   - Operações `NtCreateFile`, `NtReadFile`, `NtWriteFile`, `SetFilePointer`, `NtDeviceIoControlFile` e `NtClose` integradas ao VFS com validações estritas de bounds e ponteiros convidados.
   - Rejeição imediata e transparente de I/O assíncrono não suportado (`OVERLAPPED != 0`) com retorno canônico `STATUS_NOT_IMPLEMENTED` sem comprometer ou invalidar descritores abertos.
6. **Diagnósticos Bounded e Registro de Exportações**:
   - `ExportRegistry` com estatísticas de despacho (`RegistryStats`) e anel delimitado de diagnósticos (`UnsupportedExportInfo`, até 32 entradas e até 8 argumentos de pilha capturados), garantindo teto rígido de consumo de memória para chamadas não implementadas.
7. **Modularidade e Limite de Linhas**:
   - Refatoração dos despachantes em handlers especializados por domínio (`handlers_memory.cpp`, `handlers_time.cpp`, `handlers_sync.cpp`, `handlers_thread.cpp`, `handlers_io.cpp`).
   - Todos os arquivos do subsistema e testes mantêm-se estritamente abaixo de 500 linhas.

---

## 2. Limitações Técnicas Honestas

> [!WARNING]
> **Limitações do Subsistema Kernel**:
> - **Multithreading Cooperativo**: Escalonamento de threads convidado é cooperativo através de pontos de rendição (`NtYieldExecution`, `WaitForSingleObject`, `NtDelayExecution`, `PsTerminateSystemThread`).
> - **I/O Assíncrono Não Implementado**: Requisições com ponteiro de sobreposição (`OVERLAPPED`) retornam `STATUS_NOT_IMPLEMENTED`.
> - **Acesso a Arquivos Read-Only**: O sistema de arquivos sintetizado baseado em XDVDFS rejeita operações de gravação com `STATUS_ACCESS_DENIED`.

---

## 3. Comandos e Testes Executados

1. **Compilação Padrão (Debug)**:
   ```bash
   cmake --build build --target test_kernel -j4
   ctest --test-dir build -R test_kernel --output-on-failure
   # Resultado: 21/21 testes aprovados (100%)
   ```

2. **Suite Completa do Repositório**:
   ```bash
   ctest --test-dir build --output-on-failure
   # Resultado: 20/20 testes aprovados (100%)
   ```

3. **Compilação e Verificação com Sanitizers (ASan + UBSan)**:
   ```bash
   cmake -B build-san -DCMAKE_BUILD_TYPE=Debug -DXBLOB_ENABLE_SANITIZERS=ON
   cmake --build build-san --target test_kernel -j4
   ctest --test-dir build-san -R test_kernel --output-on-failure
   # Resultado: 21/21 testes aprovados sem vazamentos ou comportamentos indefinidos (100%)
   ```

4. **Formatação e Análise Estática**:
   ```bash
   clang-format -i libs/kernel/include/xblob/kernel/*.hpp libs/kernel/src/*.cpp tests/unit/test_kernel*
   clang-format --dry-run --Werror libs/kernel/include/xblob/kernel/*.hpp libs/kernel/src/*.cpp tests/unit/test_kernel*
   clangd --compile-commands-dir=build --check=libs/kernel/src/kernel_hle.cpp
   ```

5. **Auditoria de Limite de Linhas**:
   - `wc -l libs/kernel/include/xblob/kernel/*.hpp libs/kernel/src/*.cpp tests/unit/test_kernel*`
   - Nenhum arquivo excedeu 429 linhas (limite < 500 linhas plenamente respeitado).
