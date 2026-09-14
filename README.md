# xblob

**xblob** é um projeto de pesquisa e desenvolvimento de um emulador modular, moderno e seguro para a arquitetura do console Xbox original (2001), escrito em C/C++20 com foco em portabilidade nativa (macOS, Linux e Windows) e engenharia limpa (*clean-room*).

---

### Estado Atual do Projeto: Marco 8 (Fundação Gráfica e de Entrada Interativa, ABI C 1.6)

> [!IMPORTANT]
> **Aviso de Estado Real**: O projeto encontra-se atualmente no **Marco 8**.
> A meta final do **xblob** é carregar e executar/jogar arquivos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário.
> No **marco atual**, o emulador disponibiliza execução assíncrona e interativa experimental de títulos, com worker em background, fila de comandos thread-safe, budgets determinísticos, watchdog cooperativo, snapshots de estado, diagnóstico do primeiro bloqueador, pipeline NV2A 3D inicial, publicação bounded de frames e entrada USB OHCI/XID por teclado ou gamepad. A cobertura de IA-32 e dos serviços de kernel HLE continua incremental. **Isso não demonstra boot completo, gráficos corretos, compatibilidade ou jogabilidade de títulos comerciais**, nem garante taxa de quadros.
> Todo o conteúdo do usuário permanece estritamente local.
> O repositório e os testes NUNCA incluem, distribuem ou dependem de jogos comerciais, BIOS, chaves criptográficas, firmwares ou headers proprietários.

Para detalhes sobre o roadmap e as fronteiras de todos os subsistemas planejados, consulte [ARCHITECTURE.md](ARCHITECTURE.md). Para proveniência clean-room e fontes públicas, consulte [docs/CLEANROOM_PROVENANCE.md](docs/CLEANROOM_PROVENANCE.md). Para regras normativas de contribuição e governança de código, consulte [AGENTS.md](AGENTS.md).

---

## Recursos Implementados

- **Sessão Interativa Experimental (`libs/machine`, `libs/c_api`, `apps/desktop`)**: Integração bounded de NV2A 3D, OHCI/XID, IRQ, snapshots thread-safe, pacing/backpressure de frames e controles acessíveis de teclado/gamepad. A ABI C 1.6 adiciona contratos sized para frame, input, métricas e diagnósticos por capacidade.
- **Expansão de Instruções IA-32 (`libs/cpu`)**: Suporte a prefixos (`REP`, `REPNE`, `LOCK`, overrides de segmento), shifts/rotates (`SHL`, `SHR`, `SAR`, `ROL`, `ROR`) com preservação estrita de flags em contagem 0, multiplicação inteira (`MUL`, `IMUL` 1/2/3 operandos), divisão inteira (`DIV`, `IDIV`) com prevenção de UB no host e exceção #DE atômica, extensões e bit tests (`MOVZX`, `MOVSX`, `CDQ`, `BT`, `SETcc`), instruções de string (`MOVS`, `STOS`, `LODS`, `CMPS`, `SCAS`) com micro-stepping de `REP` delimitado por budget, e primitivas atômicas (`XCHG`, `CMPXCHG`).
- **Expansão de Serviços de Kernel HLE (`libs/kernel`)**: Implementação sintética clean-room expandida de serviços de threads e sincronização, queries de tempo, alocação de memória virtual, e despacho de armadilhas INT 0x2D integrado à sessão de máquina.
- **Sistema de Arquivos XDVDFS e Streaming (`libs/io`, `libs/formats`)**: Leitura sob demanda via `SubrangeByteSource` sem carregar a ISO inteira na memória RAM; parser iterativo defensivo de árvore binária de diretórios (BST) com detecção de ciclos, orçamentos de nós/profundidade e suporte a imagens raw (setor 32 da partição) e trimmed/XISO (setor 32/0).
- **Sistema de Arquivos Virtual VFS (`libs/vfs`)**: Normalização canônica de caminhos Xbox (`D:\`), prevenção rigorosa contra path traversal (`..`, caracteres inválidos), tabela de handles geracionais com tag de 32 bits contra use-after-free e semântica estritamente somente leitura (`IsReadOnly`).
- **Pipeline de Boot de Mídia Transacional (`libs/machine`)**: Detecção orientada a conteúdo (XBE direto ou disco XDVDFS), localização case-insensitive de `default.xbe`, carregamento em duas fases (`Plan` e `Apply`) e rollback garantido em caso de erro sem poluir o estado da sessão.
- **Serviços de Arquivo do Kernel HLE (`libs/kernel`)**: Implementação clean-room dos ordinais 190 (`NtCreateFile`), 219 (`NtReadFile`), 256 (`NtWriteFile`), 224 (`SetFilePointer`), 18 (`NtClose`), 217 (`NtQueryInformationFile`), 216 (`NtQueryDirectoryFile`), com validação atômica de memória guest e rejeição explícita de I/O assíncrono com `STATUS_NOT_SUPPORTED`.
- **ABI C Estável (`libs/c_api`)**: Interface C versionada, com ABI 1.6 retrocompatível com contratos 1.0–1.5 e capacidades NV2A 3D, OHCI, XID e sessão interativa.
- **Desktop Tauri v2 & React 19 (`apps/desktop`)**: Navegador de arquivos XDVDFS acessível (WCAG 2.2 AA) e paginado (`XdvdfsBrowser`), painel "Preparar mídia" com separação estrita em relação ao estado de gameplay interativo, e shell Rust limitado a FFI/IPC com RAII.
- **I/O Seguro e Parsing Orientado a Cursor (`libs/common`, `libs/io`)**: Leituras binárias com checagem de faixas e prevenção contra overflow aritmético.
- **Inspeção de Executáveis XBE e Detecção ISO/XISO (`libs/formats`)**: Validação estrutural de cabeçalhos mágicos, limites de seções e contêineres de disco.
- **Barramento Convidado e MMIO (`libs/bus`, `libs/memory`)**: Barramento desacoplado com roteamento de dispositivos, bancos de registradores sintéticos e adaptador MMIO.
- **Pagina\u00e7\u00e3o Virtual IA-32 4 KiB (`libs/memory`)**: Tradução de endereços virtuais via PDE/PTE (Two-Level Page Table), registros de controle CR0 (PG/WP) e CR3 (Page Directory Base), CPL (Ring 0 / Ring 3), TLB configurável e códigos arquiteturais de Page Fault (#PF).
- **Loader XBE Transacional em Duas Fases (`libs/loader`)**: Separação estrita entre planejamento (`Plan`) e aplicação (`Apply`). Toda falha durante carregamento reverte atomicamente as páginas virtuais mapeadas, garantindo rollback sem deixar estado parcial na máquina.
- **Kernel HLE Clean-Room (`libs/kernel`)**: Registro de ordinais com allowlist estrita, despachante de thunks sintéticos com leitura checked de argumentos guest, coletor sanitizado de depuração (`BufferedDebugSink`), heap determinístico com detecção de double-free, escalonador cooperativo de threads com IDs geracionais, e objetos de sincronização (eventos e mutexes recursivos) com filas FIFO reproduzíveis.
- **CPU IA-32 Estendida e Transacional (`libs/cpu`)**: Pipeline em 3 fases (`Decode` -> `Validate` -> `Commit`), decodificação completa ModR/M e SIB com limite de 15 bytes, abstração de operandos puramente virtuais sem ponteiros host, instruções de Dados (`MOV`, `LEA`), Stack (`PUSH`, `POP`, `PUSHF`, `POPF`, `CALL`, `RET`), ALU (`ADD`, `ADC`, `SUB`, `SBB`, `INC`, `DEC`, `CMP`, `AND`, `OR`, `XOR`, `TEST`) com cálculo exato de flags, Branches condicionais e incondicionais, exceções arquiteturais (`#UD`, `#GP`, `#PF`), IDTR com Interrupt/Trap Gates 32-bit e frames same-ring atômicos.
- **Sessão Determinística de Máquina e Rastreamento (`libs/machine`)**: Máquina de estados explícita (`Created`, `Prepared`, `Paused`, `Faulted`, `Stopped`) integrando todos os subsistemas, orçamentos finitos de instrução e ciclo, interrupções de GPU e buffer circular de rastreamento (`TraceRingBuffer`).
- **CLI de Diagnóstico (`apps/inspect`)**: Interface de terminal para inspecionar binários e relatar metadados ou códigos estruturados de erro (`xblob-inspect`).
- **Memória Física e Espaço de Endereçamento (`libs/memory`)**:
  - Alocação de RAM guest zero-inicializada para 64 MiB (varejo) ou 128 MiB (devkit).
  - Espaço de 32 bits com controle atômico de sobreposição de regiões.
  - Leitura/escrita de 8, 16 e 32 bits little-endian, suportando desalinhamento contido na região e impedindo cruzamento parcial de fronteiras.
  - Permissões de Leitura, Escrita e Execução, com barreira de fetch não executável.
  - Roteamento desacoplado de MMIO com checagem de largura e rejeição de acessos inválidos sem fallback para RAM.
- **Scheduler Determinístico (`libs/core`)**:
  - Relógio virtual monotônico de 64 bits (`Cycle`).
  - Fila de eventos ordenada por `(ciclo, sequência)` com desempate estável.
  - Rejeição de alvos no passado e cancelamento determinístico por `EventId` opaco.
  - Execução limitada por orçamentos de ciclo e eventos para prevenção de loops infinitos.
- **Fundação da CPU IA-32 (`libs/cpu`)**:
  - Estado arquitetural IA-32: GPRs, `EIP`, `EFLAGS` com invariante do bit 1 reservado fixo em 1 (`0x00000002` no reset), seletores mínimos e ciclo de vida.
  - Fetch transacional atômico impedindo mutações parciais em instruções truncadas.
  - Instruções de referência: `NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32` e `SUB EAX, imm32` (com `CF`, `PF`, `AF`, `ZF`, `SF`, `OF`), `JMP rel8` e `JMP rel32` com wrap de 32 bits.
  - Tratamento de opcode inválido com preservação diagnóstica do `EIP`.
  - Runner com orçamentos determinísticos de ciclos e instruções.
- **Suíte de Testes Automatizada**: Cobertura completa de unidades e teste integrado sintético reproduzível via CTest.

---

## Política de Conteúdo e Legalidade

O projeto `xblob` é estritamente comprometido com a legislação de propriedade intelectual e engenharia reversa limpa:
- **Nenhum arquivo proprietário incluso**: Não distribuímos, hospedamos nem aceitamos contribuições contendo BIOS original, ROM de MCPX, chaves criptográficas de console, jogos comerciais, firmwares ou códigos derivados do Xbox SDK oficial.
- **Fixtures Sintéticas**: Todos os testes unitários e de integração operam exclusivamente sobre dados e cabeçalhos sintéticos gerados pelo próprio projeto.
- Veja a política completa em [docs/CONTENT_POLICY.md](docs/CONTENT_POLICY.md).

---

## Compilação e Testes

### Requisitos

- Compilador C++20 compatível:
  - **macOS**: AppleClang (Xcode 14+) ou LLVM Clang 16+
  - **Linux**: GCC 11+ ou Clang 14+
  - **Windows**: MSVC 2022 (v143+) ou Clang-CL
- **CMake**: Versão 3.22 ou superior
- **Ninja** (recomendado para builds rápidos)

### Comandos de Compilação com Presets

Configuração, compilação e teste via CMake Presets:

```bash
# Preset padrão (Debug / Release)
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure

# Preset com Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer)
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure
```

---

## Arquitetura da Interface Desktop (Implementada)

Conforme governança definida em [AGENTS.md](AGENTS.md):
- **Frontend / UI**: Interface moderna, acessível (WCAG 2.2 AA) e responsiva construída em **React 19 + TypeScript**, com abas de inspeção, diagnósticos e preview de framebuffer.
- **Shell Desktop**: **Tauri v2**, empacotando o aplicativo com segurança, CSP rigoroso (`connect-src 'none'`) e baixo consumo de recursos nativos.
- **Núcleo de Emulação**: Motor escrito exclusivamente em **C/C++20**.
- **Ponte de Comunicação**: O núcleo em C/C++ expõe uma **ABI C estável** (`extern "C"`, versão 1.6). O código Rust no Tauri v2 atua estritamente como adaptador RAII de FFI/IPC, sendo expressamente proibido de conter lógica de emulação ou regras de domínio.

---

## Governança e Contribuição

- [AGENTS.md](AGENTS.md): Diretrizes para desenvolvedores e agentes autônomos.
- [ARCHITECTURE.md](ARCHITECTURE.md): Arquitetura-alvo e modelo em camadas.
- [CONTRIBUTING.md](CONTRIBUTING.md): Guia de contribuição e processo de pull request.
- [SECURITY.md](SECURITY.md): Política de divulgação responsável de vulnerabilidades.
- [docs/CONTENT_POLICY.md](docs/CONTENT_POLICY.md): Política de respeito a direitos autorais.
- [diary/](diary/): Diário de engenharia e decisões técnicas datadas.
- [LICENSE](LICENSE): Licença MIT.
