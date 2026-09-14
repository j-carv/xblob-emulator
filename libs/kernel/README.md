# Emulação de Kernel e Serviços de Sistema (libs/kernel)

O subsistema `libs/kernel` implementa a emulação de alto nível (HLE) determinística e clean-room dos serviços de sistema e thunks do kernel do Xbox.

## Responsabilidade e Diretrizes Clean-Room
- **Clean-room**: Implementado com base unicamente em documentação pública, especificações abertas e comportamento observado de interfaces NT/Xbox. Sem uso de código-fonte proprietário ou materiais confidenciais de SDKs.
- **Isolamento de Erros e Validação Transacional**: Ponteiros guest e handles são rigorosamente validados antes de qualquer mutação de estado. Operações com parâmetros inválidos retornam erros NT canônicos sem corrupção residual.
- **Determinismo**: Todos os relógios e prazos de temporização derivam unicamente da contagem de ciclos da CPU emulada (`733.333.333 Hz`), evitando qualquer deriva de relógio hospedeiro.
- **Granularidade e Limites**: Nenhum arquivo do módulo excede 500 linhas.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/memory`, `libs/cpu`, `libs/vfs`.
- **Injeção de dependências**: VFS, CPU context e AddressSpace são injetados nas chamadas de thunk, garantindo testabilidade isolada.

## Principais Componentes

### 1. Registro de Exportações e Diagnóstico Bounded
- `ExportRegistry`: Tabela associativa de exports por ordinal (1 a 378).
- `UnsupportedExportInfo` & `RegistryStats`: Rastreamento de chamadas a ordinais não implementados, gravando ordinal, TID, EIP e até 8 argumentos da pilha guest em anel delimitado (`kMaxUnsupportedHistory = 32`), mantendo consumo de memória constante.

### 2. Memória Virtual (`VirtualMemoryManager`)
- `NtAllocateVirtualMemory` (184): Alocação com `MEM_RESERVE` e `MEM_COMMIT`.
- `NtFreeVirtualMemory` (199): Liberação com `MEM_DECOMMIT` ou `MEM_RELEASE`.
- `NtQueryVirtualMemory` (220): Consulta de `MemoryBasicInformation32` com checagem de tamanho de buffer.
- `NtProtectVirtualMemory` (218): Alteração de proteção de páginas retornando permissão anterior.

### 3. Gerenciamento de Heaps (`GuestHeap` & `GuestHeapManager`)
- `RtlAllocateHeap` (273): Alocação com suporte a alinhamento e pool seguro.
- `RtlFreeHeap` (282): Liberação com validação de double-free e coalescência de blocos contíguos.
- `RtlReAllocateHeap` (290): Realocação com cópia preservada de conteúdo.
- `RtlSizeHeap` (294): Consulta de tamanho real alocado por bloco.
- `RtlCreateHeap` (277) & `RtlDestroyHeap` (279): Gestão de heaps secundários.

### 4. Tempo e Timers Determinísticos (`KernelTime`)
- Frequência base: `733'333'333 Hz`.
- `KeQueryPerformanceCounter` (108) / `KeQueryPerformanceFrequency` (109).
- `KeQuerySystemTime` (110) & `KeQueryInterruptTime` (111): Retorno em unidades FILETIME de 100 ns calculadas puramente por aritmética de ciclos.
- `KeTickCount` (144): Milissegundos determinísticos.
- `KeSetTimer` (136) & `KeCancelTimer` (95): Temporizadores one-shot e periódicos (`NotificationTimer` e `SynchronizationTimer`) com cancelamento limpo sem eventos órfãos.
- `NtDelayExecution` (194): Bloqueio com prazo em ciclos.

### 5. Threads e Escalonamento Cooperativo (`ThreadScheduler`)
- `PsCreateSystemThread` (257) & `PsCreateSystemThreadEx` (258): Criação e ciclo de vida de threads, inicialização de pilha guest e contexto salvo.
- `PsTerminateSystemThread` (262) & `NtYieldExecution` (230): Chaveamento de contexto e terminação cooperativa.
- `CheckTimeouts`: Processamento determinístico de prazos de espera expirados.

### 6. Sincronização e Handles Generacionais (`HandleTable`)
- Primitivas: Eventos (`EventObject`), Mutexes/Mutants (`MutexObject` com contagem recursiva e validação de dono), Semáforos (`SemaphoreObject` com limite máximo e fila FIFO).
- Invalidação Generacional: A tabela de handles incrementa a geração a cada `CloseHandle`, garantindo que handles obsoletos ou reutilizados sejam rejeitados de forma segura.
- `WaitForSingleObject` (90) & `WaitForMultipleObjects` (91): Espera com suporte a `WaitAny` e `WaitAll`, prazos em ciclos e tratamento atômico de falhas.

### 7. I/O de Arquivos e Dispositivos (`KernelFileServices`)
- Operações síncronas prioritárias integradas ao VFS: `NtCreateFile` (190), `NtReadFile` (219), `NtWriteFile` (256), `SetFilePointer` (224), `NtDeviceIoControlFile` (196), `NtClose` (18).
- `DeviceIoControl`: Geometria de disco/CD-ROM com buffers estruturados.
- Assincronismo honesto: Parâmetros `OVERLAPPED` não nulos são rejeitados de imediato com `STATUS_NOT_IMPLEMENTED`, preservando a integridade dos descritores de arquivo.
