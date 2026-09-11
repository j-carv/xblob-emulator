## 1. Decoder e operandos IA-32

## 1. Decoder e operandos IA-32

- [x] 1.1 Refatorar fetch/decode/commit em componentes pequenos preservando instruções existentes; verificar regressões, limite de 15 bytes e zero efeitos em truncamento.
- [x] 1.2 Implementar decoder ModR/M e SIB 32-bit com displacement/immediate checked; verificar todas as combinações base/index/scale e casos especiais.
- [x] 1.3 Implementar operand abstraction register/immediate/memory sobre VirtualMemory sem ponteiros host; verificar reads/writes e fault atomicity.
- [x] 1.4 Implementar MOV e LEA nas formas 32-bit selecionadas; verificar endereços efetivos, memória paginada e flags preservadas.
- [x] 1.5 Implementar PUSH/POP/PUSHF/POPF/CALL/RET com stack transacional; verificar underflow/wrap, falha de página e retorno nested.
- [x] 1.6 Implementar ADD/ADC/SUB/SBB/INC/DEC/CMP e helpers puros CF/PF/AF/ZF/SF/OF; verificar vetores zero/sign/carry/borrow/overflow.
- [x] 1.7 Implementar AND/OR/XOR/TEST e branches JZ/JNZ/JC/JNC/JS/JNS/JO/JNO; verificar flags afetadas/preservadas e rel8/rel32.
- [x] 1.8 Documentar opcodes/formas/ciclos suportados e adicionar testes diferenciais ou vetores publicamente verificáveis por família.

## 2. Exceções e interrupções

- [x] 2.1 Definir `CpuException` e separar #UD/#GP/#PF de erros host; verificar vector, error code, CR2/fault address e fault EIP.
- [x] 2.2 Implementar IDTR e parsing de interrupt/trap gates 32-bit com validação de limit/present/type/DPL; verificar gates válidos e inválidos.
- [x] 2.3 Implementar criação transacional de frame same-ring e transferência de controle; verificar nenhuma mutação parcial em stack fault.
- [x] 2.4 Implementar IF, CLI/STI suportados, fila IRQ mascarável determinística e entrega em instruction boundary; verificar IRQ pending com IF=0/1.
- [x] 2.5 Implementar IRET same-ring e rejeição explícita de privilege transitions/task gates; verificar round trip e frames malformados.
- [x] 2.6 Integrar scheduler/machine à fonte estreita de interrupções sem dependência CPU→machine; verificar ordem ciclo/sequência e budgets.

## 3. Kernel HLE clean-room

- [x] 3.1 Ativar `libs/kernel` com dependency graph permitido, tipos de status/handle e registry por ordinal; verificar duplicatas, unknown e ausência de dependência proprietária.
- [x] 3.2 Implementar reader/writer de argumentos guest checked e dispatcher de thunk/trap reservado; verificar ponteiros, strings limitadas e retorno ao guest.
- [x] 3.3 Implementar debug output bounded para sink injetável, sem stdout global obrigatório; verificar UTF-8/bytes inválidos, truncamento e determinismo.
- [x] 3.4 Implementar heap guest determinístico com alinhamento, split/coalesce e validação de free; verificar overlap, exhaustion, double-free e memória hostil.
- [x] 3.5 Implementar contextos e lifecycle de threads cooperativas com IDs geracionais; verificar create/exit/yield e isolamento de registradores/stacks.
- [x] 3.6 Implementar events e mutexes mínimos, wait/signal/release e filas FIFO estáveis; verificar wake order, invalid handles e ownership.
- [x] 3.7 Documentar proveniência pública/clean-room e auditar repositório/fixtures contra SDK, headers, binários, chaves e dados proprietários.

## 4. Loader, sessão e tracing

- [x] 4.1 Estender plano XBE para import thunks sintéticos allowlisted sem endereços host; verificar supported, duplicate, unknown e rollback.
- [x] 4.2 Implementar perfil explícito de elegibilidade diagnóstica, mantendo mídia arbitrária não executável; verificar spoof/malformed e mensagens honestas.
- [x] 4.3 Integrar KernelHle e thread scheduler em MachineSession mantendo-a delegadora e lifecycle válido; verificar pausa em unsupported/fault/deadlock/budget.
- [x] 4.4 Implementar trace ring buffer bounded para instruções, exceptions, HLE e thread switches; verificar overflow/drop count e ausência de dados host sensíveis.
- [x] 4.5 Criar fixtures E2E programáticas cobrindo call/stack/branches, DbgPrint, heap, duas threads, event/mutex, IRQ/IRET e término.
- [x] 4.6 Executar cada fixture duas vezes e comparar estado, ciclos, status, output e trace byte a byte.

## 5. ABI e interface diagnóstica

- [x] 5.1 Evoluir ABI C para 1.2 com capability, structs sized e APIs bounded de execução/trace; verificar consumidores legados 1.0/1.1 e C11 compile smoke.
- [x] 5.2 Implementar Rust RAII/IPC apenas como adaptação DTO e cancelamento, sem lógica HLE; verificar leaks/panics, fmt, clippy e tests locked.
- [x] 5.3 Adicionar React diagnostic runner claramente rotulado, budgets obrigatórios, cancelamento, output/trace e estados acessíveis; não adicionar Play/Run para mídia geral.
- [x] 5.4 Verificar bridge mock/real, keyboard/focus, axe WCAG 2.2 AA, reduced motion, erros e arquivo não elegível.

## 6. Integração e qualidade

- [x] 6.1 Atualizar CMake/install/export, CI macOS/Linux/Windows e checks para novos targets sem duplicate-link warnings.
- [x] 6.2 Executar warnings-as-errors, clang-format, clangd, CTest normal e ASan+UBSan incluindo malformed/fault/rollback.
- [x] 6.3 Executar npm ci/lint/typecheck/test/build e cargo fmt/clippy/test/build --locked; verificar artefatos continuam ignorados.
- [x] 6.4 Revisar dependency graph, arquivos >500 linhas, atomicidade, budgets, CSP/capabilities e conteúdo proprietário; corrigir achados.
- [x] 6.5 Atualizar README, ARCHITECTURE, documentação de CPU/kernel/proveniência e diário com comandos, limites e próximos passos honestos.
- [x] 6.6 Validar `add-kernel-hle-execution-foundation` com OpenSpec strict e confirmar todas as tarefas somente após critérios reproduzíveis.