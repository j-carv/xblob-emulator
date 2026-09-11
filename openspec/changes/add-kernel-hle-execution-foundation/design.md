## Context

Marco 3 prepara XBE sintético e executa poucas instruções. CPU não possui decoder reutilizável, pilha/chamadas/flags completas nem exceções arquiteturais. Não há kernel HLE ou threads guest. O novo marco deve aumentar fidelidade sem introduzir código proprietário, execução irrestrita ou acoplamento entre CPU e serviços de alto nível.

## Goals / Non-Goals

**Goals:** decoder transacional ModR/M/SIB; conjunto IA-32 fundamental; #UD/#GP/#PF e IDT básica; HLE clean-room modular; threads cooperativas; tracing e execução diagnóstica limitada.

**Non-Goals:** kernel oficial, compatibilidade comercial, SMP, modo real/v86, privilege transitions, syscall completo, GPU/áudio, JIT ou precisão cycle-exact.

## Decisions

### Pipeline de instrução em três fases

Separar fetch/decode, cálculo/validação de operandos e commit. `DecodedInstruction` e helpers ficam em arquivos por responsabilidade; efeitos somente após todos os reads/writes validarem. Memory destinations usam transação existente. Limite arquitetural de 15 bytes.

### Semântica por famílias

Opcode tables despacham handlers pequenos. Operandos abstraem register/immediate/memory sem armazenar ponteiro host. Helpers de flags puros calculam CF/PF/AF/ZF/SF/OF para 32 bits. Custos de ciclos são determinísticos, documentados e não alegam timing real.

### Exceções são dados arquiteturais

Criar `CpuException` com vector, optional error code e fault EIP. Falhas da MMU convertem para #PF; opcode inválido para #UD; gates/segmentos inválidos para #GP. Erros internos host permanecem `Error`. Entrega IDT é componente CPU de baixo nível; políticas HLE não entram na CPU.

### Interrupt controller mínimo

Machine mantém fila de IRQ determinística ligada ao scheduler. CPU consulta apenas uma interface estreita na fronteira entre instruções. Escopo inicial aceita IRQ mascarável same-ring e explicitamente rejeita privilege stack switch, task gates, NMI/APIC/PIC real.

### Kernel HLE isolado

Ativar `libs/kernel` dependente de common, memory e core, nunca de UI/formats. Registry mapeia ordinais sintéticos a handlers. Chamadas atravessam um trap/thunk reservado e dispatcher da machine, não ponteiros host. Serviços ficam separados em debug, heap, threading e synchronization.

### Threads guest cooperativas

`ThreadContext` contém CPU context, estado e wait reason. Scheduler HLE usa fila FIFO estável e um único host thread. Heap usa allocator guest determinístico com metadata host validada, sem expor endereços host. Handles são IDs geracionais de 32 bits.

### Perfil diagnóstico elegível

Loader somente materializa thunks para fixture marcada por metadado sintético controlado pelo projeto e imports permitidos. Mídia arbitrária continua inspeção/preparação, sem execução pela UI. ABI C 1.2 usa structs sized e handles opacos. Trace é ring buffer bounded e serializado por two-call.

### Qualidade e legalidade

Cada família de opcode tem vetores de borda. Integração roda duas vezes e compara trace/estado. Fixtures são geradas em C++/scripts. Documentar links para Intel SDM público e recursos open/clean-room sem copiar SDK Xbox. Manter CI C++/sanitizers, C11 ABI, Rust e React.

## Risks / Trade-offs

- Decoder crescer como god file → separar decoder, effective address, flags e famílias, limite ~500 linhas.
- Atomicidade de stack/IDT → planejar writes e commit transacional.
- HLE incorreto parecer compatível → nomes/escopo explícitos, unsupported por padrão e UI diagnóstica.
- Deadlocks guest → budgets de passos/eventos e estado blocked observável.
- ABI quebrada → minor bump, `struct_size`, testes com consumidor 1.0/1.1.

## Migration Plan

1. Refatorar decoder preservando regressões.
2. Adicionar operandos, flags e instruções por famílias.
3. Introduzir exceções/IDT/IRQ e testes atômicos.
4. Implementar registry e serviços HLE isolados.
5. Integrar thunks, threads, machine e trace.
6. Estender ABI/UI diagnóstica.
7. Executar todas as suítes e atualizar arquitetura, proveniência e diário.