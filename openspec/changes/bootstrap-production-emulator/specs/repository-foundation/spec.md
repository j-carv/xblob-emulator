## Purpose

Estabelece um repositório sustentável, verificável e genuinamente portável para evolução de longo prazo do emulador como produto público.

## ADDED Requirements

### Requirement: Build multiplataforma reproduzível
O projeto SHALL oferecer uma configuração CMake capaz de configurar e compilar o código C e C++ suportado em macOS, Linux e Windows, sem caminhos absolutos ou pressupostos exclusivos do host.

#### Scenario: Configuração em sistema suportado
- **WHEN** um colaborador configura o projeto com uma versão documentada do CMake e toolchain suportada
- **THEN** a configuração conclui sem exigir edição manual de arquivos do repositório

### Requirement: Fronteiras modulares explícitas
O repositório SHALL separar frontend, núcleo de emulação, dispositivos, plataforma, formatos e infraestrutura compartilhada em alvos e diretórios com responsabilidades documentadas, evitando dependências circulares e arquivos concentradores de responsabilidades.

#### Scenario: Inclusão de um novo subsistema
- **WHEN** um colaborador consulta a arquitetura para adicionar um subsistema
- **THEN** ele encontra uma fronteira, direção de dependência e localização de código inequívocas

### Requirement: Qualidade automatizada
O repositório SHALL executar build e testes automatizados em macOS, Linux e Windows, e SHALL fornecer verificações locais documentadas de formatação e análise estática.

#### Scenario: Pull request altera código portátil
- **WHEN** uma alteração é submetida ao fluxo de integração
- **THEN** builds e testes são executados nos três sistemas operacionais suportados

### Requirement: Governança e rastreabilidade
O projeto SHALL manter regras de contribuição específicas em `AGENTS.md`, um plano arquitetural na raiz e um diário Markdown versionado com decisões, progresso, comandos executados, resultados e próximos passos.

#### Scenario: Incremento de implementação concluído
- **WHEN** um agente ou colaborador conclui um incremento
- **THEN** o diário contém registro datado suficiente para outro colaborador retomar o trabalho

### Requirement: Distribuição pública legalmente segura
O repositório SHALL usar licença e dependências adequadas à distribuição pública e MUST NOT incorporar BIOS, firmware, chaves, jogos, Xbox SDK proprietário ou outros artefatos protegidos.

#### Scenario: Teste precisa de conteúdo Xbox
- **WHEN** um teste automatizado requer uma entrada no formato do console
- **THEN** ele usa bytes sintéticos produzidos no teste ou conteúdo redistribuível com proveniência documentada
