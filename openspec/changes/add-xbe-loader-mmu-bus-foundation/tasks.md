## 1. Barramento convidado

- [x] 1.1 Criar target `xblob_bus`, interfaces e faults sem dependência de machine/memory; verificar grafo CMake e compile.
- [x] 1.2 Implementar registro atômico de ranges e despacho read/write por offset/largura; verificar overlap, overflow, unmapped e encaminhamento único.
- [x] 1.3 Implementar dispositivo register-bank explicitamente sintético e adaptador MMIO; verificar round trips e logs determinísticos.

## 2. Memória virtual IA-32

- [x] 2.1 Definir estado mínimo CR0/CR3/CPL, tipos de acesso e page fault code documentado; verificar valores e encoding em testes.
- [x] 2.2 Implementar walk PDE/PTE 4 KiB little-endian com paginação on/off; verificar tradução, offsets, PDE/PTE ausente e ranges físicos inválidos.
- [x] 2.3 Implementar permissões effective read/write/user/execute disponíveis no escopo e faults com endereço; verificar matriz supervisor/user e read/write.
- [x] 2.4 Implementar cache/invalidação somente se necessário, preservando observação de PTE alterada após invalidate; verificar remapeamento determinístico.
- [x] 2.5 Integrar fetch/read/write virtual à CPU sem ponteiros host ou ciclo de dependência; verificar programa sintético através de página virtual.

## 3. Loader XBE transacional

- [x] 3.1 Criar target `xblob_loader` e modelo de plano imutável de headers/seções/permissões/entry point; verificar que planejamento não modifica memória.
- [x] 3.2 Validar integralmente offsets, endereços virtuais, raw/virtual sizes, alinhamento, overlap e overflow; verificar tabela de XBEs sintéticos maliciosos.
- [x] 3.3 Implementar cópia de headers/seções e zero-fill apenas após plano válido; verificar conteúdo exato e rollback total em falha tardia.
- [x] 3.4 Converter flags suportadas em permissões conservadoras e rejeitar requisitos/imports/TLS fora de escopo; verificar código fetchable não gravável e unsupported explícito.
- [x] 3.5 Produzir contexto inicial com entry point/base/stack sintética sem executar; verificar valores e ausência de side effects da CPU.

## 4. Sessão de máquina

- [x] 4.1 Criar target `xblob_machine` com ownership de subsistemas e lifecycle created/prepared/paused/faulted/stopped; verificar todas as transições válidas/inválidas.
- [x] 4.2 Implementar preparação em sessão nova/isolada e garantir independência entre duas sessões; verificar memória, callbacks e eventos não compartilhados.
- [x] 4.3 Implementar step/run sintético com budgets sincronizando ciclos do scheduler; verificar halt, fault, limite e determinismo em duas execuções.
- [x] 4.4 Manter `MachineSession` fina e delegadora; verificar dependências, responsabilidades e unidades abaixo de 500 linhas.

## 5. ABI e UI diagnóstica

- [x] 5.1 Estender versão minor/capabilities da ABI C com handle/DTO de preparação e estado, preservando compatibilidade 1.x; verificar consumidor com struct antigo.
- [x] 5.2 Implementar wrapper Rust RAII e comando IPC de diagnóstico sem lógica de loader; verificar fmt/clippy/test e liberação em erros.
- [x] 5.3 Adicionar painel React de validação/preparação com entry point, mappings e limitações, sem Play/Run; verificar loading/sucesso/erro, axe e bridge mock/real.

## 6. Qualidade e documentação

- [x] 6.1 Integrar testes bus/MMU/loader/machine/ABI ao CTest com fixtures exclusivamente sintéticas; verificar suíte padrão completa.
- [x] 6.2 Executar ASan+UBSan, format check e clangd em cada novo target; corrigir warnings, UB e leaks.
- [x] 6.3 Executar npm lint/typecheck/test/build e Cargo fmt/clippy/test locked após extensão da UI/FFI; corrigir regressões.
- [x] 6.4 Expandir CI macOS/Linux/Windows para novos targets e validar sintaxe/dependências sem afirmar runners não executados localmente.
- [x] 6.5 Atualizar README, ARCHITECTURE e READMEs com estado real e fontes técnicas públicas, sem alegar kernel/jogos.
- [x] 6.6 Criar diário datado com decisões, comandos, resultados, limitações e próximo marco.
- [x] 6.7 Revisar diff, ownership, casts, aritmética, atomicidade, god files e conteúdo legal; validar `add-xbe-loader-mmu-bus-foundation --strict` e marcar somente critérios cumpridos.