# Diário de Bordo: Barramento Convidado, Paginação IA-32, XBE Loader Transacional e Sessão de Máquina

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação integral da change OpenSpec `add-xbe-loader-mmu-bus-foundation` (27 tarefas).

---

## 1. Visão Geral e Objetivos do Marco

Este marco expandiu a fundação de emulação limpa do **xblob** com quatro pilares arquiteturais fundamentais:
1. **Barramento Convidado (`libs/bus`)**: Infraestrutura desacoplada para roteamento de dispositivos e periféricos através de interfaces abstratas `BusDevice` e dispositivos sintéticos.
2. **Paginação Virtual IA-32 de 4 KiB (`libs/memory`)**: MMU arquitetural com suporte a tabelas de páginas em dois níveis (PDE e PTE), registros de controle CR0 (`PG`, `WP`) e CR3 (`PDBR`), verificação de privilégios (`CPL 0` vs `CPL 3`), Translation Lookaside Buffer (TLB) com invalidação explícita e geração fiel de códigos de erro de Page Fault (`#PF`).
3. **Loader XBE Transacional em Duas Fases (`libs/loader`)**: Separação completa entre validação estática (`Plan`) e mapeamento atômico em memória virtual (`Apply`), com suporte a reversão atômica (*rollback*) em qualquer falha de alocação ou integridade.
4. **Sessão Determinística de Máquina (`libs/machine`)**: Orquestrador de ciclo de vida (`Created`, `Prepared`, `Paused`, `Faulted`, `Stopped`) integrando memória, barramento, paginação, CPU IA-32 e relógio determinístico em arquivos enxutos (<150 linhas).
5. **Evolução da ABI C para 1.1 e Integração Desktop**: Extensão da ABI C com negociação de capacidades, wrapper Rust seguro com RAII (`SafeMachineSession`), e aba diagnóstica na UI Desktop React 19 para validação de layout de memória sem controles de execução de jogos.

---

## 2. Decisões Arquiteturais e de Engenharia

### 2.1. Barramento Convidado Desacoplado (`libs/bus`)
- Criada a interface polimórfica `BusDevice` com operações `Read8/16/32` e `Write8/16/32`.
- O barramento (`Bus`) gerencia rotas de endereços físicos em faixas exclusivas, rejeitando sobreposições durante o registro.
- Implementado `SyntheticRegisterBank` para facilitar testes de dispositivos virtuais sem acoplamento com silício real.

### 2.2. Paginação Virtual IA-32 de 4 KiB (`VirtualMemory`)
- Implementada tradução de endereços virtuais baseada em diretórios e tabelas de páginas IA-32:
  - Endereço virtual decomposto em: `pde_index` (bits 31:22), `pte_index` (bits 21:12) e `offset` (bits 11:0).
  - Flags arquiteturais: Present (`bit 0`), Read/Write (`bit 1`), User/Supervisor (`bit 2`), Accessed (`bit 5`), Dirty (`bit 6`).
  - Bit de proteção contra escrita do supervisor (`CR0.WP`, bit 16): quando ativo, anula permissão de escrita em páginas de usuário mesmo no Ring 0.
- Mecanismo de TLB interno para aceleração de traduções, com métodos `InvalidateTlb` e `InvalidatePage` determinísticos.
- Falhas de página (#PF) geram códigos estruturados contendo bits `P`, `W/R`, `U/S`, `RSVD` e `I/D`.
- As escritas virtuais de múltiplos bytes (`WriteVirtualBytes`) são estritamente transacionais: todas as páginas do intervalo são validadas quanto a presença e permissão de escrita antes de qualquer mutação física na RAM.

### 2.3. Loader XBE Transacional em Duas Fases (`XbeLoader`)
- **Fase 1 (`Plan`)**:
  - Valida conformidade do cabeçalho XBE, endereços virtuais base e tamanhos das seções.
  - Exige alinhamento em páginas de 4 KiB (`0x1000`).
  - Aplica a política estrita de segurança W^X: nenhuma seção pode possuir simultaneamente permissão de escrita e execução.
  - Não produz efeitos colaterais na memória.
- **Fase 2 (`Apply`)**:
  - Mapeia páginas virtuais na `VirtualMemory` e grava os dados da seção.
  - Em caso de qualquer falha (por exemplo, colisão de página já mapeada ou esgotamento de memória física), executa rollback atômico de todas as páginas mapeadas nesta operação, restaurando o estado original intacto.

### 2.4. Sessão Determinística de Máquina (`MachineSession`)
- A sessão atua como coordenadora central e segue uma máquina de estados explícita:
  `Created -> Prepared -> Paused / Running -> Faulted / Stopped`.
- O método `PrepareXbe` integra a inspeção do binário, planejamento do loader e aplicação na memória virtual.
- Execução passo-a-passo (`Step`) e em lotes com orçamento (`RunWithBudget`), garantindo determinismo temporal coordenado com o `DeterministicScheduler`.

### 2.5. ABI C Estável 1.1 e Retrocompatibilidade
- Atualizada a versão menor da ABI para `1.1.0`.
- Estruturas contêm campos de controle de tamanho (`struct_size`) permitindo que clientes compilados contra a versão 1.0 continuem operando sem quebras de layout.
- Adicionadas funções de ciclo de vida da máquina: `xblob_machine_create`, `xblob_machine_destroy`, `xblob_machine_prepare_xbe`, `xblob_machine_get_state`, `xblob_machine_get_diagnostic`.
- Todas as chamadas de API C são isoladas com barreiras `noexcept` e tratamento de erros explícito por códigos de status (`xblob_status_t`).

### 2.6. Shell Tauri v2 & Frontend React 19
- **Rust FFI**: O módulo `SafeMachineSession` utiliza RAII (`Drop`) garantindo que o handle nativo da máquina seja liberado deterministicamente na destruição do objeto Rust, mesmo em casos de panic ou erro.
- O comando Tauri `prepare_machine_diagnostic` opera estritamente como ponte de IPC, sem replicar lógica de loader ou de emulação no lado Rust.
- **Frontend**: Adicionada a aba diagnóstica "Validação & Preparação" no `InspectionView.tsx`, detalhando:
  - Ponto de entrada virtual formatado em hexadecimal;
  - Quantidade de seções mapeadas e tamanho de cabeçalhos/imagem;
  - Mapeamento de memória física (64 MiB retail);
  - Alertas informativos sobre os limites técnicos e legais.
  - Estritamente desprovida de botões de execução ("Play" ou "Run").
  - Aprovada em auditoria de acessibilidade automatizada (axe-core: 0 violações).

---

## 3. Comandos Executados e Resultados dos Testes

| Categoria | Comando | Resultado |
| :--- | :--- | :--- |
| **Formatação C++** | `./tools/check-format.sh` | Aprovado (100% dos arquivos em conformidade) |
| **Compilação e Testes C++** | `cmake --build build && ctest --test-dir build` | 14/14 suítes aprovadas (100%) |
| **Sanitizers C++ (ASan+UBSan)** | `cmake --build build-san && ctest --test-dir build-san` | 14/14 suítes aprovadas (100%, 0 leaks, 0 UB) |
| **Rust Shell (Tauri)** | `cargo fmt --check && cargo clippy --locked && cargo test --locked` | Aprovado (0 warnings, 5/5 testes passaram) |
| **Frontend Web (React)** | `npm run lint && npm run typecheck && npm test && npm run build` | Aprovado (0 warnings, 25/25 testes passaram, build de produção OK) |
| **Compatibilidade de ABI C** | Testes de regressão `test_c_api` e `test_c_api_smoke` | Aprovado (ABI 1.0 e 1.1 validadas) |

---

## 4. Limitações Técnicas Atuais e Conformidade Legal

- **Somente Fixtures Sintéticas**: Todos os testes e validações foram realizados com executáveis sintéticos gerados em memória pela biblioteca de fixtures.
- **Sem Jogos Comerciais**: O emulador não é capaz de rodar nenhum jogo de Xbox comercial nem binários do Xbox SDK oficial.
- **Sem Kernel Xbox Oficial**: Nenhuma imagem de `xboxkrnl.exe` ou BIOS proprietária é utilizada, requerida ou suportada neste marco.
- **Sem GPU / Gráficos**: A emulação gráfica da NV2A permanece como escopo de marcos futuros.

---

## 5. Próximo Marco

- **Marco 4: Fundação de Kernel HLE (`libs/kernel`)**:
  - Implementação da infraestrutura de simulação do kernel de alto nível (HLE) para despachar chamadas de sistema através da tabela de exportações do Xbox (`xboxkrnl.exe`).
  - Abstrações de sincronização do kernel (semáforos, mutexes, threads) integradas ao `DeterministicScheduler`.
  - Mecanismo seguro de chamadas HLE com interceptação de pontos de entrada sintéticos.
