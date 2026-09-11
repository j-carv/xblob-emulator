## Why

A fundação de mídia do Marco 1 precisa evoluir para uma máquina convidada mínima e determinística antes que kernel, GPU ou jogos possam ser integrados com segurança. Este incremento introduz memória, tempo virtual e interpretação x86-32 de referência em escopo pequeno, explícito e profundamente testado.

## What Changes

- Implementar memória física convidada configurável em 64 MiB ou 128 MiB, com acessos little-endian, validação de faixas, permissões por região e falhas estruturadas.
- Introduzir um espaço de endereçamento de 32 bits com mapeamentos explícitos de RAM e uma interface de MMIO testável, sem implementar ainda paginação x86 completa.
- Implementar scheduler por ciclos inteiros, fila de eventos estável e ordenação determinística para eventos no mesmo instante.
- Modelar o estado inicial da CPU IA-32: registradores gerais, `EIP`, `EFLAGS`, segmentos mínimos e estados running/halted/faulted.
- Implementar fetch/decode/execute de um subconjunto inicial documentado: `NOP`, `HLT`, `MOV r32, imm32`, `ADD EAX, imm32`, `SUB EAX, imm32`, saltos relativos curto/próximo e opcode inválido.
- Atualizar CMake, arquitetura, README e diário para distinguir precisamente o novo estado implementado da emulação completa ainda futura e registrar React + Tauri v2 como direção obrigatória da futura interface desktop, com lógica de emulação preservada no núcleo C++.
- Adicionar testes unitários e de integração com programas exclusivamente sintéticos em memória, incluindo limites, permissões, overflows, flags, branches, faults e determinismo.
- Manter CI, warnings e sanitizers aplicados aos novos targets.
- Atualizar `AGENTS.md` para orientar changes OpenSpec geralmente substanciais, permitir delegação imediata após validação, encadear automaticamente o próximo marco após revisão bem-sucedida e tornar obrigatória uma interface React moderna, acessível e escalável com lógica do emulador isolada no C++, reduzindo interrupções artificiais sem eliminar checkpoints, critérios ou pausas por decisões materiais, riscos e bloqueios reais.

## Capabilities

### New Capabilities

- `guest-memory`: Memória física e espaço de endereçamento convidado seguro, com permissões e MMIO explícitos.
- `deterministic-scheduler`: Relógio virtual e despacho reproduzível de eventos por ciclos.
- `x86-cpu-foundation`: Estado IA-32 e intérprete inicial de referência para programas sintéticos controlados.

### Modified Capabilities

_Nenhuma._

## Impact

A mudança ativa `libs/memory`, `libs/core` e `libs/cpu`, adiciona dependências direcionais entre esses targets, amplia testes/CI/documentação e atualiza a política de orquestração do repositório em `AGENTS.md`. Não altera a CLI pública de inspeção nem carrega XBE para execução; kernel HLE, paginação x86, interrupções, FPU/MMX/SSE, JIT, GPU e execução de jogos continuam fora deste incremento.