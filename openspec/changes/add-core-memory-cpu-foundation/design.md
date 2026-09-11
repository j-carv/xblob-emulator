## Context

O Marco 1 fornece `libs/common`, I/O defensivo, parsers e uma CLI, mas não possui estado de máquina convidada. Já existem diretórios reservados para `libs/memory`, `libs/core` e `libs/cpu`; suas dependências normativas são, respectivamente, common; common/platform/memory; e common/memory. Este incremento precisa criar contratos que possam sustentar MMU, dispositivos e JIT futuros sem implementar esses componentes prematuramente.

## Goals / Non-Goals

**Goals:**

- Introduzir uma primeira vertical executável exclusivamente com programas sintéticos em memória.
- Fazer falhas de memória e CPU serem valores explícitos, reproduzíveis e testáveis.
- Separar armazenamento físico, tradução/mapeamento de endereços, tempo virtual e semântica de instrução.
- Definir semântica suficiente para expandir a CPU instrução por instrução sem despachante monolítico.
- Manter builds com warnings como erros, sanitizers e CI nos três hosts.

**Non-Goals:**

- Executar XBE, BIOS, kernel ou jogos.
- Implementar paginação x86, TLB, modos real/protegido completos, exceções arquiteturais, interrupções, FPU, MMX, SSE ou JIT.
- Modelar temporização cycle-accurate do Pentium III neste momento; os custos iniciais são determinísticos e explicitamente provisórios.
- Implementar threads ou tempo de parede do host.
- Expor nova CLI pública ou implementar a interface React/Tauri neste incremento; esta change apenas fixa sua direção arquitetural para o próximo marco.

## Decisions

### Três targets sem dependência circular

Ativar `xblob_memory`, `xblob_core` e `xblob_cpu`. Memory depende de common; core, neste incremento, depende apenas de common; CPU depende de common e memory. A coordenação entre CPU e scheduler fica em um teste/harness de integração, evitando que scheduler conheça CPU ou que CPU conheça o relógio global. Alternativa rejeitada: uma classe `Machine` já agregando tudo, pois anteciparia kernel/dispositivos e criaria um god object.

### RAM física separada do espaço de endereçamento

Uma classe de RAM possui bytes contíguos de 64/128 MiB e operações bounds-checked. Um espaço de endereçamento mantém regiões ordenadas e não sobrepostas que encaminham acessos para RAM ou MMIO. Endereço convidado é `uint32_t`; offsets e fim de faixas são calculados em tipo maior/verificado. Nenhum endereço vira ponteiro do host. Alternativa rejeitada: reservar diretamente 4 GiB no host, por custo, portabilidade e mistura de tradução com armazenamento.

### Permissões no mapeamento

Read/write/execute são flags do mapeamento, validadas antes do backend. Fetch de CPU é uma operação distinta de read para aplicar execute. Uma operação multibyte deve resolver integralmente uma única região antes de qualquer efeito, garantindo atomicidade em falha. MMIO recebe offset relativo e largura enum tipada; callbacks retornam `Result`.

### Scheduler por heap e sequência monotônica

Eventos usam ciclo alvo `uint64_t`, id opaco e número de sequência. A prioridade é `(ciclo, sequência)`, garantindo ordem de inserção em empate. Cancelamento pode ser lazy com conjunto de ids, desde que a API preserve comportamento e faça limpeza controlada. Execução aceita limites explícitos de ciclo e quantidade de eventos. Alternativa rejeitada: timestamps de `steady_clock`, pois introduzem não determinismo e dependência do host.

### CPU dividida por estado, decode e execução

Separar tipos de estado/flags, fetch-decode e handlers sem criar um arquivo central excessivo. O primeiro decoder trata apenas opcodes de byte simples e imediatos little-endian necessários às specs. `MOV r32, imm32` cobre `B8+rd`; aritmética usa `05` e `2D`; saltos usam `EB` e `E9`; `NOP` usa `90`; `HLT` usa `F4`. Cada step trabalha sobre uma cópia/transação lógica suficiente para não aplicar mudanças antes de completar fetch/decode.

### Semântica IA-32 explícita

`EIP` avança com wrap de 32 bits conforme a arquitetura, mas fetch precisa continuar validando cada byte. Cálculo de `CF`, `PF`, `AF`, `ZF`, `SF` e `OF` usa inteiros de largura fixa e fórmulas sem overflow assinado em C++. Bits não afetados são preservados; o bit reservado 1 de EFLAGS permanece definido após reset. Opcode inválido e falha de fetch guardam o EIP inicial.

### Ciclos determinísticos, não cycle-accurate

Definir tabela pequena de custos no código/documentação e testar estabilidade. A API retorna ciclos consumidos por step; um runner respeita limites antes de iniciar a próxima instrução e diferencia halted, faulted e budget-exhausted. Custos poderão mudar em uma futura spec de precisão temporal.

### Testes sintéticos e diferenciais internos

Testes unitários cobrem cada região/opcode/flag e casos-limite. Programas são arrays de bytes construídos em teste. Um teste de integração executa a mesma sequência duas vezes e compara estado, eventos e ciclos. Não usar binários produzidos pelo Xbox SDK nem dados extraídos de hardware proprietário.

### Interface desktop futura: React, Tauri v2 e núcleo C++

A interface pública desktop será construída em React + TypeScript, com design system moderno, acessibilidade, navegação clara e estado organizado por domínios. O shell escolhido é Tauri v2 para reduzir footprint e superfície em comparação ao Electron. Como Tauri requer Rust no host, esse código será um adaptador mínimo: lifecycle da janela, comandos IPC e chamada a uma ABI C versionada sobre o núcleo C++. Toda emulação, parsing, scheduler e estado da máquina permanecem em C/C++; React nunca acessa memória guest diretamente.

Alternativas consideradas: Electron é maduro, mas exige runtime Chromium/Node e processo principal JS/TS; Qt WebEngine permitiria host C++, porém adicionaria uma distribuição Chromium pesada e complexidade de licenciamento/empacotamento; wrappers nativos separados para WebView2/WKWebView/WebKitGTK evitariam Rust, mas triplicariam manutenção e comportamento web. Uma futura change específica deverá criar o workspace frontend, contrato IPC tipado, mocks, testes e packaging nos três hosts.

### Documentação honesta e fluxo OpenSpec eficiente

Atualizar arquitetura e README para “Marco 2 parcial/fundação de execução sintética”, sem afirmar emulação de software Xbox. Criar ou atualizar diário datado com decisões, comandos, resultados e limitações.

Atualizar `AGENTS.md` para estabelecer que changes OpenSpec devem, na maior parte dos casos, agrupar incrementos substanciais e coerentes, com critérios completos, em vez de microchanges que interrompam continuamente o fluxo. Após criar ou atualizar e validar estritamente os artefatos, o orquestrador pode delegar a implementação no mesmo pedido, sem exigir confirmação intermediária. Quando houver autorização de desenvolvimento contínuo, uma change concluída e revisada inicia automaticamente o planejamento, validação e delegação do próximo incremento inequívoco do roadmap. Continuam obrigatórios checkpoints por change, diário, revisão e pausas diante de decisões materiais ausentes, bloqueios, riscos ou falhas persistentes.

## Risks / Trade-offs

- [Subconjunto x86 pode cristalizar API insuficiente] → Manter decoder/handlers internos e expor apenas estado, step, runner e faults estáveis.
- [Contiguidade de 128 MiB pesa em sanitizers] → Alocação explícita e testes majoritariamente em 64 MiB; medir memória do CI.
- [MMIO via callbacks pode custar desempenho] → Priorizar contrato correto agora e medir antes de especializar dispatch.
- [Atomicidade de instrução pode ficar incompleta] → Fetch de todos os operandos antes de mutar estado; testes com instruções truncadas.
- [Flags IA-32 são propensas a erro] → Casos de fronteira tabelados e fórmulas apenas unsigned; futura validação diferencial ampliará cobertura.
- [Nome `libs/cpu` diverge do diagrama antigo `libs/cpu_x86`] → Padronizar documentação no diretório já criado `libs/cpu` nesta change.
- [Scheduler permite callbacks que agendam eventos] → Limite obrigatório de eventos no método de execução e testes de reagendamento imediato.
- [Tauri adiciona Rust a um projeto centrado em C/C++] → Restringir Rust ao shell/adaptador, manter ABI C estável e proibir lógica de domínio fora do núcleo.
- [Frontend e núcleo podem evoluir de forma acoplada] → Contrato IPC versionado, DTOs explícitos, mocks e testes de compatibilidade na change dedicada da UI.

## Migration Plan

1. Ativar memory com RAM, mappings, permissões, faults e MMIO, acompanhado de testes.
2. Ativar core com relógio/eventos determinísticos e testes.
3. Ativar cpu em camadas de estado, decode, handlers e runner, acompanhado de testes de opcode/flags/faults.
4. Adicionar integração CPU-memória-scheduler sem acoplar os targets.
5. Atualizar CMake, CI/tooling, documentação e diário.
6. Executar builds padrão e sanitizer, CTest, clang-format e clangd nos novos arquivos.

Não há consumidores externos a migrar. Se um target falhar, ele pode ser removido isoladamente do CMake sem alterar a vertical de inspeção do Marco 1.