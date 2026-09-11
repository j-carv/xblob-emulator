## 1. Contratos e build

- [x] 1.1 Definir faults/códigos de erro e tipos de largura fixa compartilhados necessários a memória, scheduler e CPU sem quebrar APIs existentes; verificar compilação e testes de common.
- [x] 1.2 Substituir os READMEs placeholder por targets `xblob_memory`, `xblob_core` e `xblob_cpu` com includes públicos e dependências direcionais do design; verificar o grafo de links no CMake e build dos três targets.
- [x] 1.3 Atualizar `AGENTS.md` com a política de changes OpenSpec geralmente grandes/coesas, delegação imediata após validação e continuidade autônoma entre changes; registrar também React como interface obrigatória, Tauri v2 como shell selecionado e a regra de manter lógica de emulação no núcleo C++ com Rust limitado ao adaptador; verificar que as regras não autorizam pular planejamento, validação, revisão ou checkpoints.

## 2. Memória convidada

- [x] 2.1 Implementar RAM física configurável em 64 MiB/128 MiB, zerada e sem capacidade arbitrária; verificar testes de tamanho, zero inicial, capacidade inválida e último byte.
- [x] 2.2 Implementar regiões e espaço de endereçamento convidado de 32 bits com rejeição atômica de sobreposição e faixa inválida; verificar testes de mapeamentos adjacentes, sobrepostos, unmapped e overflow no fim de `uint32_t`.
- [x] 2.3 Implementar leitura/escrita de 8/16/32 bits little-endian com validação integral da faixa; verificar round trips, acesso desalinhado permitido, cruzamento de região e ausência de escrita parcial.
- [x] 2.4 Implementar permissões read/write/execute e operação específica de instruction fetch; verificar violações de leitura, escrita e execução e preservação dos bytes em falha.
- [x] 2.5 Implementar backend MMIO tipado com offset relativo, largura e propagação de `Result`; verificar contagem de callbacks, valores, largura rejeitada e ausência de fallback para RAM.

## 3. Scheduler determinístico

- [x] 3.1 Implementar relógio monotônico `uint64_t`, avanço verificado e agendamento por ciclo/id/sequência; verificar avanço exato, alvo no passado e overflow.
- [x] 3.2 Implementar despacho por `(ciclo, sequência)` com ordem estável em empates; verificar eventos fora de ordem de inserção, simultâneos e futuros.
- [x] 3.3 Implementar cancelamento por id opaco sem perturbar eventos restantes; verificar cancelamento válido, repetido e de id desconhecido.
- [x] 3.4 Implementar execução com limites de ciclo e quantidade de eventos, incluindo reagendamento no ciclo atual; verificar retorno `limit reached` sem loop infinito.

## 4. Fundação da CPU IA-32

- [x] 4.1 Implementar estado arquitetural IA-32, registradores gerais, EIP, EFLAGS com bit 1 reservado fixo em 1, segmentos mínimos e ciclo de vida; verificar valores após reset e invariante do bit 1.
- [x] 4.2 Implementar fetch/decode transacional de NOP, HLT e MOV r32, imm32 com avanço atômico de EIP e falhas em limite/permissão; verificar mutação de registradores, avanço de EIP e ausência de mutação parcial em falha.
- [x] 4.3 Implementar ADD EAX, imm32 e SUB EAX, imm32 com cálculo exato de CF, PF, AF, ZF, SF e OF; verificar resultados aritméticos, flags e overflow com sinal.
- [x] 4.4 Implementar JMP rel8 e JMP rel32 com deslocamento com sinal e wrap modular de 32 bits; verificar saltos positivos, negativos e wrap de borda.
- [x] 4.5 Implementar tratamento de opcode inválido com preservação diagnóstica do estado e EIP original; verificar transição para faulted, erro estruturado e distinção de falha de fetch.
- [x] 4.6 Implementar tabela determinística de ciclos por instrução e runner com orçamentos de instrução e ciclo; verificar contagem determinística, parada por halt, por fault e por esgotamento de orçamento.

## 5. Integração e qualidade

- [x] 5.1 Criar programa sintético integrado CPU+memória+scheduler e executá-lo duas vezes; verificar igualdade de registradores, memória, ordem de eventos e ciclos finais.
- [x] 5.2 Integrar os novos testes ao CTest e aplicar warnings/sanitizers aos targets; verificar build/test padrão e ASan+UBSan sem warnings ou falhas.
- [x] 5.3 Executar clang-format check e `clangd --check` em pelo menos uma unidade nova de cada target; corrigir todos os diagnósticos verificáveis.
- [x] 5.4 Atualizar `README.md`, `ARCHITECTURE.md` e READMEs dos subsistemas para o estado real do Marco 2, corrigindo comandos de presets divergentes e documentando a futura arquitetura React + Tauri v2 + ABI C/C++; verificar que não existe alegação de execução de XBE, kernel, interface já implementada ou jogos.
- [x] 5.5 Criar/atualizar diário datado com decisões, arquivos, comandos, resultados, limitações e próximos passos; verificar consistência com testes realmente executados e plataformas apenas cobertas por CI.
- [x] 5.6 Revisar diff, dependências, ownership, casts, aritmética, tamanhos de arquivo e ausência de dados proprietários/god files; executar `openspec validate add-core-memory-cpu-foundation --strict` e confirmar todos os critérios antes de marcar a change concluída.
