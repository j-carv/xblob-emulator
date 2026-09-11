# Arquitetura do Emulador xblob (ARCHITECTURE.md)

Este documento descreve a organização arquitetural do emulador **xblob**, discriminando categoricamente o **estado atual implementado** da **arquitetura-alvo completa**, acompanhado de diagramas conceituais, fronteiras de subsistemas, contratos de dependência, fluxo de execução e roadmap por marcos.

---

## 1. Estado Atual Implementado vs. Arquitetura-Alvo

| Dimensão | Estado Atual (Marco 4: Execução IA-32 Estendida, Exceções, Kernel HLE e ABI 1.2) | Arquitetura-Alvo Completa |
| :--- | :--- | :--- |
| **Escopo** | I/O seguro, inspeção defensiva de mídias, barramento convidado, paginação IA-32 de 4 KiB, loader transacional, CPU IA-32 transacional com ALU/Stack/Branch, IDT e exceções, Kernel HLE isolado, trace buffer limitado, ABI C 1.2 e UI diagnóstica | Emulação completa de hardware e software do console Xbox original (2001) |
| **Execução de Código** | Execução diagnóstica de instruções IA-32 sintéticas com orçamentos determinísticos. **Nenhuma execução de jogos comerciais, kernel oficial ou títulos completos** | CPU x86 (intérprete de referência e JIT dinâmico), HLE de Kernel e LLE seletivo |
| **Subsistemas Ativos** | `libs/common`, `libs/bus`, `libs/io`, `libs/formats`, `libs/memory`, `libs/core`, `libs/cpu`, `libs/kernel`, `libs/loader`, `libs/machine`, `libs/c_api`, `apps/inspect`, `apps/desktop` | Frontend/UI, Scheduler, CPU, MMU, NV2A, APU/MCPX, USB/Input, PCI, Kernel HLE, FATX |
| **Segurança e I/O** | Cursores sem casts, limites estritos de faixas, isolamento de RAM guest, validação de permissões, paginação protegida, rollback atômico do loader e sem ponteiros host em operandos da CPU | Isolamento total de processos, sandboxing de I/O, verificação de integridade |
| **Suporte a Mídia** | Executáveis XBE e imagens ISO/XISO estruturalmente inspecionáveis e executáveis XBE sintéticos executáveis via perfil explícito de diagnóstico | Discos físicos, dumps ISO/XISO, partição de HDD FATX, cartões de memória |

> [!IMPORTANT]
> **Aviso de Honestidade Técnica**: O projeto encontra-se atualmente no **Marco 4**. Nenhuma tentativa de carregar jogos proprietários ou executar o kernel oficial é realizada. O escopo atual limita-se à fundação de execução e Kernel HLE clean-room para fixtures diagnósticas e testes de conformidade.

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
      v (C ABI 1.1)
[libs/c_api]
      |
      v
[libs/machine] (MachineSession: Created, Prepared, Paused, Faulted, Stopped)
      +---------------+---------------+---------------+
      |               |               |               |
      v               v               v               v
[libs/loader]    [libs/cpu]      [libs/core]     [libs/bus]
      |               |                               |
      +-------+-------+                               v
              v                                [libs/memory]
       [libs/memory] <--------------------------------+
  (VirtualMemory 4KiB & AddressSpace)
              |
              v
        [libs/common]
```

As dependências são estritamente unidirecionais:
- `xblob_common`: Raiz fundamental de tipos, erros e aritmética segura.
- `xblob_bus`: Barramento convidado, dispositivos desacoplados, registradores sintéticos. Depende apenas de `xblob_common`.
- `xblob_memory`: Espaço físico (`AddressSpace`, `Ram`), MMIO tipado, paginação virtual IA-32 (`VirtualMemory`) com TLB e tradução PDE/PTE de 4 KiB, e adaptador de barramento. Depende de `xblob_common` e `xblob_bus`.
- `xblob_core`: Scheduler determinístico e relógio monotônico. Depende apenas de `xblob_common`.
- `xblob_cpu`: CPU IA-32 de referência (GPRs, EIP, EFLAGS, CR0/CR3/CPL, instruções, runner com orçamento). Depende de `xblob_common` e `xblob_memory`.
- `xblob_loader`: Planejamento puro e carregamento transacional de executáveis XBE com rollback atômico. Depende de `xblob_common`, `xblob_formats` e `xblob_memory`.
- `xblob_machine`: Coordenador determinístico de sessão de máquina (<150 linhas/arquivo). Depende de `xblob_common`, `xblob_bus`, `xblob_core`, `xblob_memory`, `xblob_cpu` e `xblob_loader`.
- `xblob_c_api`: ABI C estável `extern "C"` versionada (1.1). Depende de `xblob_common`, `xblob_formats` e `xblob_machine`.

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
- Conjunto inicial de instruções (`NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32`, `SUB EAX, imm32`, `JMP rel8`, `JMP rel32`).
- Runner determinístico com orçamentos de instruções e ciclos.

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
4. **Escopo Legal & Técnico**: A aplicação é estritamente uma ferramenta de inspeção e preservação histórica de mídia. Nenhuma ação de execução/emulação de jogos comerciais é permitida ou oferecida.

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
- **Marco 3: Barramentos, MMU e Carregador Inicial de Memória**
  - Mapeamento de paginação de hardware de 4 KB.
  - Barramento PCI host-to-PCI e configuração de dispositivos.
  - Carregador inicial de seções XBE para memória virtual guest.
- **Marco 4: Kernel HLE Fundamental e Serviços de Sistema**
  - Implementação inicial de exports de `xboxkrnl.exe` (threads, sincronização, heap, DbgPrint).
  - Execução controlada de pequenos programas de diagnóstico em memória.
- **Marco 5: GPU NV2A Mínima e Pushbuffer**
  - FIFO Pushbuffer da GPU NV2A e processamento de comandos.
  - Apresentação de buffers de quadro no host.
- **Marco 6: Sistema de Arquivos FATX, Entradas e Áudio MCPX**
  - Parser completo de volumes FATX para o VFS.
  - Emulação de controlador USB Xbox (gamepads).
  - Áudio estéreo básico MCPX / AC97.
- **Marco 7: Interface Desktop Moderna e JIT Dinâmico**
  - Frontend desktop React + Tauri v2 integrado ao núcleo via C ABI.
  - Compilador JIT x86-para-ARM64 / x86-para-x86_64.
