# Arquitetura do Emulador xblob (ARCHITECTURE.md)

Este documento descreve a organização arquitetural do emulador **xblob**, discriminando categoricamente o **estado atual implementado** da **arquitetura-alvo completa**, acompanhado de diagramas conceituais, fronteiras de subsistemas, contratos de dependência, fluxo de execução e roadmap por marcos.

---

## 1. Estado Atual Implementado vs. Arquitetura-Alvo

| Dimensão | Estado Atual (Marco 6: Sistema de Arquivos XDVDFS, VFS Virtual, Boot de Mídia e Kernel File Services HLE) | Arquitetura-Alvo Completa |
| :--- | :--- | :--- |
| **Escopo** | I/O seguro, streaming subrange, leitor XDVDFS defensivo, VFS read-only com handles geracionais, pipeline de boot por conteúdo com rollback, barramento convidado, cabeçalho PCI Type 0, paginação IA-32 de 4 KiB, loader transacional, CPU IA-32 transacional, Kernel HLE com serviços de arquivos, GPU NV2A com superfície linear RGBA8 e pushbuffer, trace buffer, ABI C 1.4 e UI desktop com navegador VFS e preparação de mídia | Emulação completa de hardware e software do console Xbox original (2001) para execução local de mídias fornecidas pelo usuário (.xbe, .iso, .xiso) |
| **Execução de Código** | Execução diagnóstica de instruções IA-32 sintéticas com orçamentos determinísticos, preparação transacional de executáveis XBE e imagens de disco ISO/XISO, serviços de arquivos HLE e comandos 2D via pushbuffer NV2A. **A emulação interativa de jogos comerciais é a meta final do projeto e ainda não executa neste marco** | CPU x86 (intérprete de referência e JIT dinâmico), HLE de Kernel e LLE seletivo para jogos do usuário |
| **Subsistemas Ativos** | `libs/common`, `libs/bus`, `libs/pci`, `libs/gpu`, `libs/io`, `libs/formats`, `libs/vfs`, `libs/memory`, `libs/core`, `libs/cpu`, `libs/kernel`, `libs/loader`, `libs/machine`, `libs/c_api`, `apps/inspect`, `apps/desktop` | Frontend/UI, Scheduler, CPU, MMU, NV2A, APU/MCPX, USB/Input, PCI, Kernel HLE, FATX, VFS |
| **Segurança e I/O** | Streaming subrange sem ler ISO inteira, parser iterativo com orçamentos estritos e proteção contra ciclos, bloqueio total de path traversal (`..`), handles geracionais, atomicidade com rollback e buffers guest transacionais | Isolamento total de processos, sandboxing de I/O, verificação de integridade |
| **Suporte a Mídia** | Executáveis XBE e imagens de disco ISO/XISO (raw e trimmed) montáveis via XDVDFS, com localização automática de `default.xbe`, preparação de sessão e navegação paginada | Discos físicos, dumps ISO/XISO, partição de HDD FATX, cartões de memória |

> [!IMPORTANT]
> **Aviso de Honestidade Técnica**: O projeto encontra-se atualmente no **Marco 6**. A meta final do emulador é carregar e executar títulos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário. No marco atual, o emulador monta discos XDVDFS, navega em seus arquivos, prepara sessões de máquina de forma transacional e atende chamadas de I/O do kernel convidado. **Ainda não executa jogos comerciais**, limitando-se à execução diagnóstica de fixtures sintéticas, validação estática de formatos e teste da cadeia de boot e gráficos. O repositório e os testes jamais distribuem ou dependem de jogos, BIOS, chaves, firmware ou SDKs proprietários.

---

## 2. Diagramas Arquiteturais

### 2.1. Arquitetura-Alvo em Camadas

```
+-----------------------------------------------------------------------------------+
|                                  APLICAÇÕES (apps/)                               |
|   +------------------------------------+   +----------------------------------+   |
|   |  xblob-inspect (CLI de Inspeção)   |   |  xblob-desktop (React + Tauri v2)|   |
|   +-----------------+------------------+   +----------------+-----------------+   |
+---------------------|---------------------------------------|---------------------+
                      |                                       |
                      v                                       v
+-----------------------------------------------------------------------------------+
|                              SERVIÇOS DE SISTEMA & HLE                            |
|   +------------------+   +-------------------+   +----------------------------+   |
|   |  Loader & Mídia  |   |    Kernel HLE     |   |   Scheduler Determinístico |   |
|   |  (libs/formats)  |   |   (libs/kernel)   |   |        (libs/core)         |   |
|   +--------+---------+   +---------+---------+   +--------------+-------------+   |
+------------|-----------------------|----------------------------|-----------------+
             |                       |                            |
             v                       v                            v
+-----------------------------------------------------------------------------------+
|                              SUBSISTEMAS DE HARDWARE                              |
|   +----------------+  +-----------------+  +-----------------+  +-------------+   |
|   |    CPU IA-32   |  |    NV2A GPU     |  |    APU / MCPX   |  | Barramento  |   |
|   |   (libs/cpu)   |  |   (libs/gpu)    |  |   (libs/audio)  |  |  PCI/LPC/SMB|   |
|   +--------+-------+  +--------+--------+  +--------+--------+  +------+------+   |
|            |                   |                    |                  |          |
|            +-------------------+----------+---------+------------------+          |
|                                           v                                       |
|                               +------------------------+                          |
|                               |  Memória Guest & MMIO  |                          |
|                               |      (libs/memory)     |                          |
|                               +-----------+------------+                          |
+-------------------------------------------|---------------------------------------+
                                            v
+-----------------------------------------------------------------------------------+
|                        INFRAESTRUTURA COMPARTILHADA & I/O                         |
|   +-----------------------------------+   +-----------------------------------+   |
|   |          libs/io (I/O)            |   |     libs/platform (Abstração)     |   |
|   |   (Streams, Cursores de bytes)    |   |     (Janela, Áudio, Sincronismo)  |   |
|   +-----------------+-----------------+   +-----------------+-----------------+   |
|                     |                                       |                     |
|                     +-------------------+-------------------+                     |
|                                         v                                         |
|                         +-------------------------------+                         |
|                         |    libs/common (Base Comum)   |                         |
|                         |  (Result, Erros, Aritmética)  |                         |
|                         +-------------------------------+                         |
+-----------------------------------------------------------------------------------+
```

### 2.2. Fluxo de Dados e Direção de Dependência no Marco Atual

```
[apps/desktop] (React 19 + Tauri v2 Shell FFI/IPC)
      |
      v (C ABI 1.3)
[libs/c_api]
      |
      v
[libs/machine] (MachineSession: Created, Prepared, Paused, Faulted, Stopped)
      +---------------+---------------+---------------+---------------+---------------+
      |               |               |               |               |               |
      v               v               v               v               v               v
[libs/loader]    [libs/cpu]      [libs/kernel]   [libs/gpu]      [libs/pci]      [libs/core]
      |               |               |               |               |
      +-------+-------+---------------+               v               v
              v                                [libs/bus] <-----------+
       [libs/memory] <--------------------------------+
  (VirtualMemory 4KiB & AddressSpace)
              |
              v
        [libs/common]
```

As dependências são estritamente unidirecionais:
- `xblob_common`: Raiz fundamental de tipos, erros e aritmética segura.
- `xblob_bus`: Barramento convidado, dispositivos desacoplados, registradores sintéticos. Depende apenas de `xblob_common`.
- `xblob_pci`: Barramento PCI, cabeçalho Type 0, registro determinístico e BAR sizing/routing. Depende de `xblob_common` e `xblob_bus`.
- `xblob_gpu`: Dispositivo NV2A, registradores permitidos, superfícies RGBA8 e processador de pushbuffer. Depende de `xblob_common`, `xblob_bus` e `xblob_pci`.
- `xblob_memory`: Espaço físico (`AddressSpace`, `Ram`), MMIO tipado, paginação virtual IA-32 (`VirtualMemory`) com TLB e tradução PDE/PTE de 4 KiB, e adaptador de barramento. Depende de `xblob_common` e `xblob_bus`.
- `xblob_core`: Scheduler determinístico e relógio monotônico. Depende apenas de `xblob_common`.
- `xblob_cpu`: CPU IA-32 de referência (GPRs, EIP, EFLAGS, CR0/CR3/CPL, instruções, runner com orçamento). Depende de `xblob_common` e `xblob_memory`.
- `xblob_kernel`: Kernel HLE clean-room com registro de ordinais, heap determinístico e threads. Depende de `xblob_common`, `xblob_memory`, `xblob_cpu` e `xblob_core`.
- `xblob_loader`: Planejamento puro e carregamento transacional de executáveis XBE com rollback atômico. Depende de `xblob_common`, `xblob_formats` e `xblob_memory`.
- `xblob_machine`: Coordenador determinístico de sessão de máquina integrando barramentos, CPU, kernel, PCI e GPU. Depende de `xblob_common`, `xblob_bus`, `xblob_core`, `xblob_memory`, `xblob_cpu`, `xblob_kernel`, `xblob_loader`, `xblob_pci` e `xblob_gpu`.
- `xblob_c_api`: ABI C estável `extern "C"` versionada (1.3). Depende de `xblob_common`, `xblob_formats` e `xblob_machine`.

---

## 3. Responsabilidades dos Subsistemas Implementados

### 3.1. `libs/common`
- Tipos de largura fixa (`GuestAddr`, `GuestSize`, `Cycle`, `EventId`, `ByteSpan`, `MutableByteSpan`).
- Monad de resultado seguro (`Result<T, E>`).
- Códigos e categorias de erro estruturados (`ErrorCode`).
- Aritmética defensiva (`safe_math.hpp`) com checagem de overflow e validação de faixas.

### 3.2. `libs/io`
- Leitura orientada a cursor sem type punning (`BinaryReader`).
- Abstrações de fontes de dados (`ByteSpanSource`, `FileSource`).
- Decodificação little-endian determinística.

### 3.3. `libs/formats`
- Parser defensivo para executáveis XBE sintéticos (`XbeParser`).
- Detector estrutural de mídias e setores de volume ISO/XISO (`IsoDetector`).
- Orquestrador de diagnóstico (`MediaInspector`).

### 3.4. `libs/bus`
- Interface polimórfica abstrata `BusDevice` para barramentos e dispositivos desacoplados.
- Barramento de sistema `Bus` com registro determinístico e resolução de rotas sem acoplamento a hardware físico real.
- Banco de registradores sintéticos `SyntheticRegisterBank` para testes unitários e dispositivos virtuais.

### 3.5. `libs/memory`
- `Ram`: Gerenciador de memória física de 64 MiB (retail) ou 128 MiB (devkit), zero-inicializada, com bounds checking rigoroso.
- `AddressSpace`: Mapeador de espaço de 32 bits com controle atômico contra sobreposições parciais.
- MMIO tipado (8, 16 e 32 bits) desacoplado, sem fallback silencioso para RAM.
- `VirtualMemory`: Paginação IA-32 de 4 KiB com PDE (Page Directory Entry) e PTE (Page Table Entry).
  - Suporte completo a registros CR0 (PG - Paging Enabled, WP - Write Protect) e CR3 (PDBR - Page Directory Base Register).
  - Respeito ao CPL (Current Privilege Level: Ring 0 Supervisor vs Ring 3 User).
  - TLB configurável com invalidação explícita (`InvalidateTlb`, `InvalidatePage`).
  - Falhas de página (#PF) com registro fiel do código de erro arquitetural (P, W/R, U/S, RSVD, I/D).
  - Operações transacionais de escrita virtual com reversão atômica em falha.

### 3.6. `libs/loader`
- `XbeLoader`: Loader de executáveis XBE estritamente transacional em duas fases:
  - Fase 1 (`Plan`): Validação puramente em memória e sem efeito colateral das seções, alinhamentos (4 KiB), cabeçalhos e permissões conservadoras (W^X).
  - Fase 2 (`Apply`): Mapeamento atômico de páginas virtuais e cópia dos payloads. Qualquer falha (memória esgotada, seção corrompida, conflito) dispara rollback completo, restaurando o estado original da memória virtual.
- `CreateInitialContext`: Configuração determinística do contexto IA-32 (`EIP`, pilha sintética, registradores gerais zerados e seletores válidos).

### 3.7. `libs/machine`
- `MachineSession`: Orquestrador fino e determinístico de ciclo de vida da máquina convidada.
- Estados de sessão explícitos: `Created`, `Prepared`, `Paused`, `Faulted`, `Stopped`.
- Execução passo-a-passo (`Step`) e por lotes de ciclos (`RunWithBudget`) operando em modo físico ou modo virtual paginado.

### 3.8. `libs/core`
- `DeterministicScheduler`: Relógio virtual monotônico de 64 bits (`Cycle`).
- Fila de eventos priorizada por `(ciclo, sequência)` garantindo estabilidade no desempate.
- Rejeição de alvos no passado (`ErrorCode::PastCycle`).
- Cancelamento idempotente por `EventId` opaco.
- Runner delimitado por orçamentos de ciclo e limite de eventos executados para prevenir loops infinitos em reagendamentos imediatos.

### 3.9. `libs/cpu`
- `CpuContext`: Estado arquitetural IA-32 (registradores gerais `EAX`..`EDI`, `EIP`, `EFLAGS` com invariante forçado do bit 1 reservado, seletores de segmento `CS`..`GS`, registros CR0 e CR3).
- Ciclo de vida (`Running`, `Halted`, `Faulted`).
- Fetch transacional sem mutação parcial em caso de instrução truncada ou falha de acesso à memória.
- Conjunto inicial e estendido de instruções (ALU, Stack, Mov, Branches) e tratamento de exceções arquiteturais (#UD, #GP, #PF).
- Runner determinístico com orçamentos de instruções e ciclos.

### 3.10. `libs/kernel`
- Kernel HLE clean-room com despachante de thunks por ordinais permitidos (`ExportRegistry`).
- Coletor sanitizado de logs (`BufferedDebugSink`).
- Heap determinístico com detecção de double-free.
- Escalonador cooperativo de threads e objetos de sincronização (eventos e mutexes recursivos) com filas FIFO reproduzíveis.

### 3.11. `libs/pci`
- Abstração do barramento PCI (Peripheral Component Interconnect) com endereçamento BDF.
- Cabeçalho de configuração Type 0 com acessos little-endian tipados de 8, 16 e 32 bits.
- Suporte a Base Address Registers (BARs) com protocolo de dimensionamento via `0xFFFFFFFF` e detecção de colisões.
- Gating estrito de transações MMIO através do bit `memory_space` no registrador `command`.

### 3.12. `libs/gpu`
- Dispositivo gráfico NV2A com allowlist de registradores (`PMC`, `PBUS`, `PFIFO`, `PGRAPH`, `PCRTC`).
- Superfície linear RGBA8 desacoplada de janelas nativas ou APIs gráficas do host.
- Decodificador de pushbuffer em métodos e pacotes tipados.
- Processador de comandos 2D determinístico (`CLEAR`, `FILL_RECT`, `FLIP_SURFACE`) com orçamentos estritos de ciclos e pacotes.
- Geração determinística de interrupções de GPU (`GpuInterruptSource`) integradas ao barramento e scheduler.

---

## 4. Arquitetura da Interface Desktop (Implementada)

A fundação desktop do xblob conecta a interface visual ao motor de preservação através de um pipeline vertical estritamente desacoplado:

```
+-----------------------------------------------------------------------------------+
|                        CAMADA REACT UI (apps/desktop/src)                         |
|   +---------------------------------------------------------------------------+   |
|   |  Telas: HomeScreen, InspectionView, AboutModal                             |   |
|   |  Componentes WCAG 2.2 AA: Button, Card, Badge, Tabs, Alert, Modal, SkipLink|   |
|   |  Contrato de Bridge: BridgeContract (MockBridge / TauriBridge)            |   |
|   +---------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------+
                                         |
                                         | IPC (@tauri-apps/api/core)
                                         v
+-----------------------------------------------------------------------------------+
|                   SHELL TAURI v2 & ADAPTADOR RUST (src-tauri)                     |
|   +---------------------------------------------------------------------------+   |
|   |  Comandos IPC: get_core_info, inspect_media (apps/desktop/src-tauri)      |   |
|   |  Segurança: CSP restrito, zero permissões de rede/shell/escrita arbitrária|   |
|   |  Adaptador FFI RAII: SafeMediaReport (drop automático via c_api_destroy)  |   |
|   |  ISOLAMENTO TOTAL: ZERO lógica de domínio de preservação em Rust          |   |
|   +---------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------+
                                         |
                                         | ABI C11 Pura (extern "C")
                                         v
+-----------------------------------------------------------------------------------+
|                        CORE C ABI (libs/c_api)                                    |
|   +---------------------------------------------------------------------------+   |
|   |  Header: xblob/c_api.h (C11 seguro, tipos primitivos e handles opacos)   |   |
|   |  Versionamento semântico de ABI e negociação de capacidades               |   |
|   |  Fronteira noexcept com barreira contra vazamento de exceções C++         |   |
|   |  Buffers bidirecionais (two-call pattern) e validação estrita UTF-8       |   |
|   +---------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------+
                                         |
                                         | Chamadas C++20 internas
                                         v
+-----------------------------------------------------------------------------------+
|                     NÚCLEO DE INSPEÇÃO & FORMATOS (libs/formats)                  |
|   +---------------------------------------------------------------------------+   |
|   |  MediaInspector, XbeParser, IsoDetector, BinaryReader, FileSource        |   |
|   +---------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------+
```

### 4.1. Limites de Responsabilidade e Garantias de Segurança
1. **React UI**: Puramente declarativa, WCAG 2.2 AA compliant (contraste >= 4.5:1, foco visível com offset, suporte a leitor de tela e `prefers-reduced-motion`), sem acesso a filesystem direto.
2. **Rust Shell**: Adaptador fino e DTO translator; zero parsing ou lógica de mídia reescrita em Rust.
3. **C ABI**: Fronteira C11 estável e portável; garante que clientes futuros (Python, bindings nativos) possam consumir o núcleo xblob sem quebrar compatibilidade binária.
4. **Escopo Legal & Técnico**: A meta final do produto é carregar e executar títulos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário. O repositório e os binários distribuídos são 100% livres de software proprietário (sem BIOS, sem chaves, sem jogos distribuídos). No marco atual, a execução opera em modo diagnóstico com fixtures sintéticas; o suporte interativo a jogos comerciais ainda não executa nesta versão, aguardando validação de pipelines de emulação futuros.

---

## 5. Roadmap de Evolução por Marcos

- **Marco 1: Bootstrap & Fundação de Mídia (Concluído)**
  - Monorepo modular em C/C++20 compilável em macOS, Linux e Windows.
  - Governança normativa (`AGENTS.md`), I/O seguro sem casts e aritmética verificada.
  - Parsers defensivos para XBE e detecção estrutural de ISO/XISO.
  - CLI `inspect`, fixtures sintéticas e CI multiplataforma.
- **Marco 2: Memória Guest, Scheduler e CPU IA-32 (Concluído)**
  - `libs/memory`: RAM física 64/128 MiB, espaço de 32 bits, permissões R/W/X e MMIO tipado.
  - `libs/core`: Scheduler determinístico com relógio monotônico e controle de eventos.
  - `libs/cpu`: Estado IA-32, fetch transacional, subconjunto inicial de instruções e runner com orçamentos.
  - Testes unitários abrangentes e teste de integração determinístico com dupla execução idêntica.
- **Marco 3: MMU, Paginação 4 KiB e Loader XBE Transacional (Concluído)**
  - Mapeamento de paginação IA-32 de 4 KiB (PDE/PTE) com TLB e suporte a CR0/CR3.
  - Carregador transacional em duas fases (`Plan` e `Apply`) com rollback atômico.
  - Sessão determinística de máquina (`libs/machine`).
- **Marco 4: Kernel HLE Fundamental, CPU Estendida e Exceções (Concluído)**
  - Registro de ordinais com allowlist estrita e Kernel HLE clean-room (`libs/kernel`).
  - Pipeline CPU IA-32 em 3 fases, decodificação ModR/M/SIB, ALU completa, Stack, Branches e exceções (#UD, #GP, #PF).
  - Shell desktop inicial em React 19 + Tauri v2 via ABI C 1.2.
- **Marco 5: Barramento PCI, Fundações da GPU NV2A e Framebuffer (Concluído)**
  - Barramento PCI com cabeçalho Type 0, endereçamento BDF e BAR sizing/routing (`libs/pci`).
  - Dispositivo NV2A, registradores permitidos, superfície RGBA8 e decodificador/processador de pushbuffer 2D (`libs/gpu`).
  - ABI C 1.3 com capacidades gráficas e extração de snapshots de quadros (`libs/c_api`).
  - Visualização de display via Canvas no desktop com elegibilidade de testes sintéticos (`apps/desktop`).
- **Marco 6: Sistema de Arquivos XDVDFS, VFS Virtual, Boot de Mídia e Kernel File Services HLE (Concluído)**
  - `libs/io`: `SubrangeByteSource` imutável e seguro para streaming de mídia sem carregar ISOs completas na memória.
  - `libs/formats`: Leitor XDVDFS defensivo, parsing iterativo de árvores de diretório com detecção de ciclos e bounds rígidos, suporte a ISO raw e trimmed/XISO.
  - `libs/vfs`: Virtual File System com suporte a caminhos Xbox (`D:\`), prevenção total de path traversal (`..`), handles geracionais anti-use-after-free e volumes estritamente read-only.
  - `libs/machine`: `MediaBootPipeline` por conteúdo (XBE ou XDVDFS), localização de `default.xbe`, transacionalidade com rollback e integração de ownership à `MachineSession`.
  - `libs/kernel`: Módulo `file_services` com NtCreateFile, NtReadFile, NtWriteFile, SetFilePointer, NtClose, NtQueryInformationFile, NtQueryDirectoryFile, validação prévia de buffers e rejeição de I/O assíncrono com `STATUS_NOT_SUPPORTED`.
  - `libs/c_api`: ABI C 1.4 retrocompatível, novas capacidades e navegador VFS paginado two-call.
  - `apps/desktop`: Componente React `XdvdfsBrowser` acessível (WCAG 2.2 AA) e paginado, e fluxo dedicado "Preparar mídia" no `MediaBootPanel`.
- **Marco 7: Sistema de Arquivos FATX, Entradas USB e Áudio MCPX (Próximo)**
  - Parser de partição e sistemas de arquivos FATX para o VFS.
  - Emulação de controlador USB Xbox (gamepads).
  - Áudio básico MCPX / AC97.
- **Marco 8: Pipeline de Execução Completo e JIT Dinâmico**
  - Pipeline de execução unificada para carregar e rodar jogos comerciais fornecidos pelo usuário.
  - Compilador JIT x86-para-ARM64 / x86-para-x86_64.
