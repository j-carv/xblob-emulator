# Diretrizes Normativas para Agentes e Colaboradores (AGENTS.md)

Este documento estabelece as regras mandatórias e permanentes de engenharia, governança, arquitetura e qualidade para o desenvolvimento do emulador **xblob**. Todo colaborador, humano ou agente autônomo de inteligência artificial, DEVE seguir estas instruções integralmente.

---

## 1. Princípios Fundamentais do Produto

1. **Produto Público de Longo Prazo**: O projeto é desenhado para evolução contínua ao longo de múltiplos anos, priorizando manutenibilidade, transparência e clareza estrutural sobre soluções temporárias ou atalhos técnicos.
2. **Honestidade Técnica**: Nunca alegue compatibilidade, funcionalidade ou suporte sem verificação objetiva e reproduzível. Neste marco inicial de bootstrap, o projeto implementa apenas validação, inspeção estrutural defensiva de arquivos e infraestrutura básica; a emulação e execução de código convidado estão explicitamente fora de escopo.
3. **Legalidade e Segurança Jurídica**:
   - É **estritamente proibido** incorporar, comitar, empacotar ou referenciar binários protegidos por direitos autorais, tais como BIOS original, ROMs MCPX, chaves criptográficas de console, certificados privados, imagens de recuperação, firmwares, jogos comerciais ou o Xbox SDK oficial proprietário.
   - Todo teste automatizado DEVE utilizar exclusivamente fixtures sintéticas geradas programaticamente ou dados redistribuíveis com proveniência documentada e permissiva.
   - Qualquer especificação técnica deve ser derivada de documentação pública, especificações abertas ou engenharia reversa limpa (*clean-room*).

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
