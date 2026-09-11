# Diretrizes Normativas para Agentes e Colaboradores (AGENTS.md)

Este documento estabelece as regras mandatórias e permanentes de engenharia, governança, arquitetura e qualidade para o desenvolvimento do emulador **xblob**. Todo colaborador, humano ou agente autônomo de inteligência artificial, DEVE seguir estas instruções integralmente.

---

## 1. Princípios Fundamentais do Produto

1. **Meta Final do Produto**: O objetivo final do **xblob** é carregar e executar/jogar arquivos `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário em seu ambiente local. Conteúdo comercial fornecido pelo usuário não é proibido como dado de entrada local. O que é terminantemente proibido é incorporar, distribuir, comitar ou testar no repositório qualquer jogo comercial, BIOS, chave privada, firmware, kernel proprietário ou cabeçalhos/SDKs protegidos por direitos autorais.
2. **Honestidade Técnica e Distinção entre Estado Atual e Meta Final**:
   - Nunca alegue compatibilidade, funcionalidade ou suporte sem comprovação objetiva e reproduzível.
   - O emulador **ainda não executa jogos comerciais neste marco** (o suporte interativo completo e controles "Play" dependem de pipelines de execução e renderização futuros validados objetivamente).
   - O preview gráfico e a elegibilidade diagnóstica restrita a fixtures sintéticas neste marco são limitações temporárias de engenharia incremental, não um limite permanente do produto.
   - Toda documentação, interface ou mensagem pública DEVE distinguir categoricamente o estado atual implementado da meta final do emulador, apresentando um roadmap honesto e verificável.
3. **Legalidade e Segurança Jurídica**:
   - É **estritamente proibido** incorporar, comitar, empacotar ou referenciar binários protegidos por direitos autorais, tais como BIOS original, ROMs MCPX, chaves criptográficas de console, certificados privados, imagens de recuperação, firmwares, jogos comerciais ou o Xbox SDK oficial proprietário.
   - Todo teste automatizado no repositório DEVE utilizar exclusivamente fixtures sintéticas geradas programaticamente ou dados com proveniência clean-room documentada.
   - Qualquer especificação técnica implementada deve ser derivada de documentação pública, especificações abertas ou engenharia reversa limpa (*clean-room*).

---

## 2. Portabilidade e Suporte Multiplataforma

1. **Sistemas Suportados como Cidadãos de Primeira Classe**:
   - **macOS** (AppleClang / Clang, ARM64 / x86_64)
   - **Linux** (GCC / Clang, x86_64 / ARM64)
   - **Windows** (MSVC / Clang-CL, x86_64)
2. **Encapsulamento de Plataforma**:
   - É proibido espalhar `#ifdef _WIN32`, `#ifdef __APPLE__` ou `#ifdef __linux__` em lógica de domínio, parsing ou emulação.
   - Detalhes do sistema operacional DEVEM ser encapsulados em bibliotecas e contratos de abstração (`libs/platform/` ou módulos dedicados de I/O).
   - Não assuma endianness de host, tamanho de tipos de ponteiro nem alinhamento implícito de compilador. Use tipos de largura fixa (`uint8_t`, `uint32_t`, `uint64_t`, etc.) e conversões explícitas de little-endian.
3. **Caminhos e I/O**:
   - Nunca utilize caminhos absolutos no código-fonte, build ou testes.
   - Use separadores de caminho portáveis e bibliotecas padrão (`std::filesystem`).

---

## 3. Modularidade e Proibição de God Files

1. **Estrutura Modular Monorepo**:
   - `apps/`: Aplicações e frontends (CLI fina, interfaces de usuário). Não contêm lógica de domínio nem parsers embutidos.
   - `libs/`: Bibliotecas estáticas ou compartilhadas com responsabilidade única e dependências estritamente direcionais:
     - `libs/common/`: Tipos fundamentais, erros estruturados, utilitários sem dependências externas.
     - `libs/io/`: Abstração de leitura de bytes, arquivos e cursores binários seguros.
     - `libs/formats/`: Parsers defensivos de contêineres e formatos de mídia (XBE, ISO, XISO).
     - *(Subsistemas futuros)*: `libs/cpu/`, `libs/memory/`, `libs/kernel/`, `libs/gpu/`, `libs/audio/`, `libs/bus/`, etc.
   - `tests/`: Suíte de testes unitários e de integração organizados por subsistema.
   - `cmake/`: Módulos e configurações de compilação reutilizáveis.
   - `docs/`: Documentação de arquitetura, subsistemas e especificações.
   - `tools/`: Scripts de tooling, formatação e automação local.
2. **Dependências Direcionais e Acíclicas**:
   - Bibliotecas de nível inferior não podem depender de bibliotecas de nível superior. Exemplo: `libs/common` e `libs/io` nunca dependem de `libs/formats` ou `apps`.
   - Nenhuma biblioteca conhece detalhes de aplicação ou apresentação.
3. **Proibição Estrita de God Files**:
   - Cada arquivo deve ter uma única responsabilidade clara (*Single Responsibility Principle*).
   - Como diretriz objetiva, unidades de tradução não devem ultrapassar ~500 linhas de código sem justificativa documentada. Arquivos que acumulam múltiplos subsistemas, despachantes gigantes ou estruturas globais são expressamente vedados.

---

## 4. Padrões de Código e Linguagem (C/C++20)

1. **Padrão**: C++20 como linguagem principal de domínio; C para interfaces de baixo nível ou FFI onde justificado.
2. **Parsing Defensivo e Segurança de Memória**:
   - Trate **toda entrada externa** (arquivos de jogo, imagens, executáveis) como potencialmente hostil e malformada.
   - **Proibido cast de buffers para structs de host**: `reinterpret_cast<const Header*>(buffer)` é expressamente proibido devido a undefined behavior com alinhamento, endianness, type punning e tamanhos de tipos.
   - Toda leitura de dados binários DEVE utilizar cursores orientados a bytes com verificação de limites (*bounds checking*) e checagem de overflow aritmético (`offset + size <= total_size`) antes de qualquer acesso.
   - Não use exceções nas fronteiras internas públicas de parsing. Utilize tipos de retorno explícitos de resultado/erro (como `Result<T, Error>`).
3. **Formatação e Estilo**:
   - Configuração mantida em `.clang-format` e `.editorconfig`.
   - Nomes claros e autoexplicativos; documentação Doxygen/Javadoc para contratos de API.

---

## 5. Qualidade, Testes e CI

1. **Suíte Automatizada CTest**:
   - Todos os testes devem ser executáveis sem acesso à rede e sem dependências externas proprietárias.
   - Testes unitários devem cobrir casos nominais, arquivos truncados, assinaturas corrompidas, limites exatos e overflows aritméticos.
2. **Sanitizers e Análise Estática**:
   - Suporte a AddressSanitizer (`ASan`) e UndefinedBehaviorSanitizer (`UBSan`) nos compiladores suportados (Clang e GCC).
   - Warnings tratados de forma rigorosa (`-Wall -Wextra -Wpedantic` no GCC/Clang; `/W4 /permissive-` no MSVC).
3. **Integração Contínua (CI)**:
   - Workflows automatizados de build e teste para macOS, Linux e Windows em cada alteração.

---

## 6. Manutenção Obrigatória do Diário de Desenvolvimento

1. **Atualização Mandatória em `diary/`**:
   - Cada sessão de trabalho ou implementação de incremento DEVE gerar ou atualizar um registro no diretório `diary/`, nomeado no formato `YYYY-MM-DD-*.md`.
   - O registro deve conter obrigatoriamente:
     - Contexto e decisões tomadas;
     - Progresso realizado;
     - Comandos executados e resultados de testes;
     - Limitações conhecidas e próximos passos recomendados.

---

## 7. Arquitetura da Interface Desktop (Futura)

1. **Frontend Moderno em React**:
   - A futura interface gráfica de usuário (UI) do emulador DEVE ser desenvolvida obrigatoriamente em **React** com **TypeScript**, proporcionando interface moderna, amigável (*user-friendly*), acessível e escalável, com divisão clara de estado por domínio.
2. **Shell Desktop Selecionado (Tauri v2)**:
   - O empacotador/shell selecionado é **Tauri v2**, priorizando baixo footprint de memória e superfície reduzida em comparação a runtimes baseados em Electron ou distribuições embutidas de Chromium.
3. **Isolamento de Domínio e FFI C/Rust**:
   - Todo o núcleo de emulação, gerenciamento de memória guest, decodificação/execução de instruções, agendamento determinístico e parsing binário DEVE residir e permanecer no núcleo **C/C++**.
   - O código em **Rust** introduzido pelo Tauri v2 DEVE ser estritamente restrito à camada de casca/adaptador (*shell adapter*): gerenciamento do ciclo de vida da janela desktop, IPC com o frontend React e invocação do núcleo através de uma **ABI C estável e versionada**.
   - É **expressamente proibido** implementar regras de negócio da emulação, lógica de hardware virtual ou interpretação de formatos no Rust ou no frontend React. O frontend nunca acessa diretamente a memória guest.

---

## 8. Governança do Fluxo OpenSpec e Operação de Agentes

1. **Granularidade das Changes OpenSpec**:
   - As changes OpenSpec DEVEM ser planejadas, na grande maioria dos casos, como incrementos **substanciais e coesos** (verticais completas com especificações, design e tarefas detalhadas), evitando microchanges excessivamente fragmentadas que geram fricção desnecessária e interrupções artificiais no fluxo de desenvolvimento.
2. **Delegação Imediata após Validação**:
   - Assim que uma change for criada ou atualizada e obtiver validação estrita bem-sucedida (`openspec validate <change-id> --strict`), o agente orquestrador DEVE delegar a implementação imediatamente na mesma sessão de trabalho, sem requerer confirmações intermediárias redundantes do usuário.
3. **Continuidade Autônoma entre Changes com Checkpoints**:
   - Quando concedida autorização para desenvolvimento contínuo, a conclusão integral e revisão rigorosa de uma change encadeia automaticamente o planejamento, validação e delegação do próximo incremento inequívoco do roadmap.
4. **Preservação Rígida de Controles e Critérios de Parada**:
   - A continuidade autônoma e a delegação imediata **NÃO autorizam em hipótese alguma** pular etapas normativas: planejamento de artefatos, validação estrita (`--strict`), execução de testes, revisão de diffs e registros de checkpoints no diário de engenharia (`diary/`) continuam estritamente obrigatórios.
   - O agente DEVE obrigatoriamente pausar a execução e solicitar direcionamento do usuário somente diante de:
     - Necessidade de tomada de decisão técnica ou arquitetural material não coberta pelas especificações;
     - Riscos imprevistos que afetem premissas fundamentais de design ou segurança;
     - Bloqueios externos ou falhas de compilação/teste persistentes que não possam ser sanadas de forma determinística e fundamentada.

---

## 9. Protocolo Normativo de Orquestração Paralela de Agentes com Git Worktrees

Este protocolo estabelece as regras permanentes e mandatórias para execução simultânea de agentes autônomos utilizando worktrees Git isolados e o protocolo ACP (`ask_antigravity`).

### 9.1. Critérios de Paralelização, Base Limpa e Decomposição Parent/Lane
1. **Estrutura Parent/Lane**: Todo marco paralelo DEVE possuir uma change OpenSpec pai (integração e arquitetura) e sub-changes/planos derivados para cada *lane* de trabalho com escopo funcional bem definido (ex.: `cpu`, `kernel`, `gpu`, `audio`, `ui`).
2. **Requisito de Base Limpa e Imutável (`BASE_SHA`)**:
   - Todas as lanes DEVEM ser criadas a partir do mesmo commit checkpoint limpo e validado (`BASE_SHA`).
   - O workspace principal DEVE estar rigorosamente limpo (`git status --porcelain` vazio).
   - É terminantemente proibido abrir worktrees paralelos sobre um workspace sujo ou com alterações não commitadas. Havendo trabalho em andamento, as alterações DEVEM ser previamente revisadas, testadas e commitadas na árvore principal antes de iniciar qualquer lane paralela.
3. **Limite de Concorrência e Escalonamento**:
   - O limite padrão mandatório é de **no máximo 3 lanes concorrentes simultâneas** para preservar estabilidade do host (CPU, memória e contenção de I/O) e evitar sobrecarga de contexto.
   - A ampliação além de 3 lanes requer autorização explícita e monitoramento comprovado de recursos.

### 9.2. Ownership Disjunto, DAG de Dependências e Hotspots Compartilhados
1. **Grafo Acíclico Dirigido (DAG)**:
   - Apenas tarefas e lanes cujas dependências mútuas forem nulas no DAG podem ser executadas simultaneamente.
   - Caso uma lane dependa de abstrações ou saídas de outra, a execução DEVE ser serializada.
2. **Ownership Estrito e Disjunto**:
   - Cada lane possui ownership exclusivo sobre seus arquivos de subsistema (ex.: `libs/cpu/`, `libs/gpu/`, etc.). Nenhuma lane pode editar arquivos fora do seu escopo atribuído.
3. **Hotspots Reservados Exclusivamente à Integration Lane**:
   - Arquivos estruturais, de coordenação e contratos globais são classificados como **hotspots protegidos** e JAMAIS podem sofrer edição concorrente em lanes paralelas:
     - CMake raiz: `CMakeLists.txt`
     - C ABI pública e implementação FFI: `libs/c_api/include/xblob/c_api.h`, `libs/c_api/src/c_api.cpp`
     - Composição e orquestração de máquina: `libs/machine/include/xblob/machine/machine_session.hpp`, `libs/machine/src/machine_session.cpp`
     - Arquivos de lock e metadados de dependência: `apps/desktop/package-lock.json`, `Cargo.lock`
     - Documentação global e governança: `AGENTS.md`, `README.md`, `ARCHITECTURE.md`
   - Se uma lane necessitar de alteração em um hotspot compartilhado, a modificação DEVE ser extraída e executada previamente ou posteriormente pela **integration lane** na árvore principal, ou ser tratada como pré-requisito serial.

### 9.3. Invocação ACP via `ask_antigravity`, Sessões e Retomada
1. **Isolamento por `cwd`**:
   - O orquestrador DEVE invocar subagentes ACP via `ask_antigravity` especificando obrigatoriamente o `cwd` apontando para o diretório exclusivo do worktree correspondente (`.worktrees/<lane>`).
   - É proibido invocar múltiplos agentes compartilhando o mesmo `cwd`.
2. **Modelo Mandatório**:
   - Subagentes executores em lanes paralelas DEVEM ser configurados obrigatoriamente com o modelo **Gemini 3.8 Flash High**.
3. **Operação Silenciosa**:
   - As instruções passadas ao subagente DEVEM exigir trabalho silencioso (sem narrativa intermediária, sem comentários de progresso prolixos), retornando apenas o resumo final estruturado, lista de arquivos alterados, comandos/testes executados e pendências.
4. **Registro de Task ID e Retomada**:
   - Toda sessão paralela DEVE registrar o task ID retornado pelo orquestrador no manifesto local (`.worktrees/manifest.json`).
   - Havendo interrupção de contexto, desconexão ou continuação entre sessões futuras, a retomada de uma tarefa inacabada DEVE ocorrer estritamente no mesmo worktree e no mesmo `cwd`, preservando o histórico do branch `agent/<parent>/<lane>`.

### 9.4. Commits Focados, Integração Serial e Gates Completos
1. **Commits Focados por Lane**:
   - Ao concluir suas tarefas, cada lane DEVE validar seus testes locais e gerar commits limpos e atômicos em seu branch exclusivo (`agent/<parent>/<lane>`), com mensagens explicativas no formato convencional.
   - É expressamente proibido comitar arquivos fora do ownership da lane.
2. **Integração Serial sem Force**:
   - A integração na branch principal/pai DEVE ser estritamente serial (uma lane de cada vez).
   - O orquestrador/integration lane DEVE inspecionar o diff antes de cada integração.
   - A integração deve ser realizada via merge `--no-ff` ou `git cherry-pick` serial. O uso de flags destrutivas (`--force`, `-f`, `git reset --hard`) é terminantemente proibido.
3. **Gates Completos Pós-Integração**:
   - Após a integração de cada lane, a suíte de testes afetada DEVE ser executada.
   - Após a conclusão da integração de todas as lanes do marco, TODOS os gates globais de qualidade DEVEM ser executados e aprovados antes de qualquer push:
     - CTest completo em modo release/debug;
     - CTest completo com AddressSanitizer (ASan) e UndefinedBehaviorSanitizer (UBSan);
     - Suíte Rust (`cargo test --locked`, `cargo clippy -- -D warnings`, `cargo fmt --check`);
     - Frontend desktop (`npm run lint`, `npm run typecheck`, `npm run test` / `vitest`, `npm run build`);
     - Verificação de formatação C/C++ (`./tools/check-format.sh`);
     - Validação estrita OpenSpec (`openspec validate <parent-change> --strict`).

### 9.5. Ferramentas de Checkpoint, Integração e Governança de Ownership
1. **Comando de Checkpoint Explícito (`checkpoint`)**:
   - Todo commit DEVE ser formalizado explicitamente via `agent-worktree.sh checkpoint -m "<mensagem>"`.
   - O checkpoint exige mensagem de commit mandatória, recusa staging vazio e proíbe auto-staging indiscriminado (`git add -A` implícito é vedado).
   - Executa preflight obrigatório de validação OpenSpec strict para changes afetadas e gates de qualidade; não possui opção de bypass de gates.
   - NUNCA executa `git push` automaticamente (push requer aprovação humana e verificação em main).
2. **Inicialização da Integration Branch (`init-integration`)**:
   - A branch `integration/<parent>` DEVE ser criada a partir de `BASE_SHA` limpo e comum via `agent-worktree.sh init-integration <parent> [BASE_SHA]`.
   - O comando bloqueia execução se o workspace estiver sujo ou se a branch já existir apontando para commit divergente.
3. **Validação Estrita de Ownership (`validate-ownership`)**:
   - Antes do merge de qualquer lane na branch de integração, o orquestrador DEVE executar `agent-worktree.sh validate-ownership <lane>`.
   - O comando inspeciona o diff `BASE_SHA..lane` e rejeita qualquer alteração fora dos padrões `owned` declarados ou que coincida com hotspots globais protegidos (`CMakeLists.txt`, `libs/c_api/**`, `libs/machine/**`, lockfiles, etc.).
4. **Manifesto Local Versionado (Schema 2.0)**:
   - O arquivo local `.worktrees/manifest.json` utiliza `schemaVersion: "2.0"` e registra: `parent`, `integrationBranch`, `baseSha`, bem como metadados por lane (`lane`, `branch`, `path`, `baseSha`, `createdAt`, `taskId`, `change`, `owned`, `denied`, `dependencies`).

