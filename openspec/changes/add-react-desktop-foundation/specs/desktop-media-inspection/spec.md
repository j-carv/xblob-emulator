## Purpose

Oferece uma aplicação desktop segura e compreensível para selecionar arquivos Xbox e visualizar a inspeção real produzida pelo núcleo, sem sugerir emulação ainda inexistente.

## ADDED Requirements

### Requirement: Inicialização desktop
O aplicativo SHALL iniciar em macOS, Linux e Windows com shell nativo e apresentar claramente o estado atual do projeto e as capacidades disponíveis.

#### Scenario: Primeira abertura
- **WHEN** o usuário abre o aplicativo sem mídia selecionada
- **THEN** vê uma tela inicial útil, ação de inspeção e aviso de que execução de jogos ainda não está disponível

### Requirement: Seleção nativa de mídia
O usuário SHALL poder escolher arquivo `.xbe`, `.iso` ou `.xiso` por diálogo nativo, sem conceder ao frontend acesso irrestrito ao filesystem.

#### Scenario: Usuário cancela diálogo
- **WHEN** o usuário cancela a seleção
- **THEN** o estado anterior permanece intacto e nenhum erro é exibido

### Requirement: Inspeção integrada ao núcleo
A aplicação SHALL encaminhar o caminho selecionado pelo comando IPC ao adaptador e à ABI C, exibindo o resultado real do núcleo; o frontend MUST NOT interpretar bytes da mídia.

#### Scenario: Mídia reconhecida
- **WHEN** o usuário seleciona fixture sintética válida
- **THEN** a interface apresenta tipo, caminho, tamanho e metadados disponíveis retornados pelo núcleo

#### Scenario: Mídia inválida
- **WHEN** o núcleo rejeita a mídia
- **THEN** a interface apresenta categoria e mensagem compreensível, preserva estabilidade e permite tentar novamente

### Requirement: Estados assíncronos claros
A interface SHALL distinguir estados vazio, selecionando, inspecionando, sucesso e erro, impedindo submissões duplicadas enquanto uma inspeção está ativa.

#### Scenario: Inspeção em andamento
- **WHEN** uma solicitação ainda não terminou
- **THEN** a interface mostra progresso, desabilita ação conflitante e mantém navegação segura

### Requirement: Ausência de controles enganosos
A aplicação MUST NOT apresentar ações de iniciar, jogar ou executar mídia enquanto o núcleo não possuir essa capacidade especificada.

#### Scenario: Resultado válido exibido
- **WHEN** a inspeção conclui com sucesso
- **THEN** somente ações compatíveis com inspeção estão disponíveis
