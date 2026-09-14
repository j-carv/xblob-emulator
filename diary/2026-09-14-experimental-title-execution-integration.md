# Diário de Engenharia: Integração de Execução Experimental de Títulos (2026-09-14)

## 1. Contexto e Objetivos

Este registro consolida a integração da mudança principal OpenSpec `add-experimental-title-execution` a partir das branches das lanes especializadas:
- CPU Lane (`agent/experimental-title-execution/cpu-execution` no commit `8926c24905cb4779215332b98abebd16a4567c11`), correspondente à child change `expand-ia32-title-execution`.
- Kernel Lane (`agent/experimental-title-execution/kernel-services` no commit `5fffe4f3c5070b76af4a662a4659a598f3db454b`), correspondente à child change `expand-kernel-title-services`.

A branch de integração `integration/experimental-title-execution` unificou as implementações da CPU IA-32 e dos serviços do kernel sintético HLE, implementando adicionalmente:
1. **MachineSession Execution Worker Assíncrono**:
   - Thread de execução assíncrona desacoplada da thread principal com fila de comandos thread-safe (`Start`, `Resume`, `Pause`, `Stop`, `Step`).
   - Ciclo de vida determinístico com estados `Created`, `Prepared`, `Running`, `Paused`, `Stopped` e `Faulted`.
   - Limites operacionais granulares (`ExecutionBudgets` para instruções, ciclos, wall-time ms e eventos).
   - Watchdog cooperativo e controle atômico de pausa/parada.
   - Captura thread-safe de snapshots estruturados (`MachineSnapshot`) e diagnósticos latched do primeiro bloqueador de compatibilidade (`CompatibilityDiagnostic`).
   - Rastreamento circular thread-safe (`TraceRingBuffer`) com proteção por mutex.
   - Dispatcher de armadilhas CPU INT 0x2D para invocação de thunks do kernel HLE em tempo de execução de títulos.
2. **Extensão da ABI C para Versão 1.5.0**:
   - `XBLOB_C_API_VERSION_MINOR 5`.
   - Novas capacidades: `XBLOB_CAPABILITY_EXPERIMENTAL_TITLE_EXECUTION` (bit 13) e `XBLOB_CAPABILITY_COMPATIBILITY_DIAGNOSTICS` (bit 14).
   - Structs estáveis C11 retrocompatíveis com tamanho explícito: `xblob_execution_budgets_t`, `xblob_cpu_registers_snapshot_t`, `xblob_machine_snapshot_t`, `xblob_compatibility_diagnostic_t`.
   - Exportação de funções C: `xblob_machine_start_execution`, `xblob_machine_resume_execution`, `xblob_machine_wait_completion`, `xblob_machine_get_snapshot`, `xblob_machine_get_compatibility_diagnostic` e `xblob_machine_get_trace_text`.
3. **Bindings Rust FFI, DTOs e Comandos Tauri Desktop**:
   - Wrapper seguro `SafeMachineSession` com garantia RAII no `Drop` (parada e destruição segura da sessão C).
   - Comandos assíncronos Tauri não-bloqueantes (`start_title_execution`, `resume_title_execution`, `pause_title_execution`, `stop_title_execution`, `get_execution_snapshot`, `get_compatibility_diagnostic`, `get_execution_trace`).
4. **Interface React Desktop Acessível e Responsiva**:
   - Novo painel `ExperimentalExecutionPanel` integrado ao `InspectionView`.
   - Aviso proeminente de execução experimental clean-room exigindo consentimento explícito do usuário.
   - Controles de execução completos (Iniciar, Retomar, Passo, Pausar, Parar) e configuração dinâmica de budgets.
   - Visualização em tempo real de métricas, tabela responsiva dos registradores IA-32, inspeção bounded de 8 palavras da pilha (ESP) com validação de memória e exibição clara do primeiro bloqueador de compatibilidade.

---

## 2. Decisões de Engenharia e Resolução de Conflitos

1. **Fusão Serial Sem Conflitos (Merge --no-ff)**:
   - Os merges das lanes CPU e Kernel foram efetuados sequencialmente sem conflito no git, dado que a CPU trabalhou exclusivamente em `libs/cpu` e `tests/unit/test_cpu*`, e o Kernel atuou em `libs/kernel` e `tests/unit/test_kernel.cpp`.
2. **Sincronização de Estado na Transição Start/Resume**:
   - Para prevenir condições de corrida onde `WaitCompletion()` ou consultas imediatas de snapshot poderiam observar o estado `Prepared` ou `Paused` antes da thread de trabalho assimilar o comando, `state_` é transicionado para `MachineState::Running` de forma síncrona dentro de `Start()` e `Resume()` sob a proteção do mutex `mutex_`.
3. **Inspeção de Pilha Bounded**:
   - Para garantir total segurança e evitar falhas de segmentação ou leituras inválidas de memória host, a inspeção de pilha no snapshot (`TakeSnapshotLocked`) valida se o ponteiro `ESP` atual do guest está contido nos limites do espaço de endereço virtual. Havendo memória válida, até 8 palavras de 32 bits (32 bytes) são extraídas sem vazar ponteiros de memória host.
4. **Política Clean-Room Estrita**:
   - Nenhuma BIOS, chave criptográfica ou código de SDK proprietário foi inserido. Todas as suítes de teste operam com fixtures sintéticas programáticas criadas em memória (`CreateValidSyntheticXbe`, `BuildValidTrimmedXdvdfsImage`).

---

## 3. Comprovação de Gates de Qualidade

- **CMake / CTest Padrão**: 20/20 suítes de teste aprovadas com 100% de sucesso.
- **ASan / UBSan**: Build com AddressSanitizer e UndefinedBehaviorSanitizer aprovou 20/20 testes sem qualquer vazamento de memória ou comportamento indefinido.
- **C11 Smoke Test**: Verificação retrocompatível da ABI C 1.5.0 em compilador C11 nativo (`test_c_api_smoke`).
- **Rust Tauri Backend**: 11 testes unitários aprovados (`cargo test`), `cargo clippy -- -D warnings` sem avisos, `cargo fmt` limpo e `cargo build --locked` concluído com sucesso.
- **Frontend React**: 36 testes Vitest aprovados (`npm test`), TypeScript typechecking estrito sem erros (`npm run typecheck`), ESLint sem warnings (`npm run lint`), e `npm run build` gerado com sucesso.
