## Purpose

Transporta frames RGBA produzidos pelo core para consumidores e interface desktop com ownership e memória limitados.

## ADDED Requirements

### Requirement: Surface limitada
A GPU SHALL criar surfaces RGBA8 com width, height, pitch e byte-size verificados contra overflow e limites configurados.

#### Scenario: Dimensão maliciosa
- **WHEN** dimensões ou pitch excedem limites
- **THEN** criação falha sem alocação descontrolada

### Requirement: Snapshot versionado
A ABI C SHALL expor metadata e cópia bounded do frame por handle/snapshot, com sequence number e two-call pattern, sem ponteiro interno.

#### Scenario: Buffer pequeno
- **WHEN** cliente fornece capacidade insuficiente
- **THEN** tamanho requerido é retornado e nenhum byte fora do buffer é escrito

### Requirement: Apresentação desktop
React/Tauri SHALL mostrar o último frame diagnóstico mantendo aspect ratio, estados vazio/loading/error e acessibilidade, sem lógica GPU em Rust/TypeScript.

#### Scenario: Novo frame
- **WHEN** sequence aumenta
- **THEN** UI atualiza canvas/imagem e libera buffers anteriores de modo limitado

### Requirement: Elegibilidade honesta e evolutiva
Neste marco, frames SHALL ser disponíveis somente para sessão diagnóstica elegível; mídia do usuário ainda não suportada não ganha ação Play nem promessa prematura de renderização. A arquitetura MUST preservar a evolução para execução futura de `.xbe`, `.iso` e `.xiso` fornecidos pelo usuário.

#### Scenario: Mídia ainda não suportada
- **WHEN** arquivo fornecido pelo usuário não satisfaz as capacidades implementadas neste marco
- **THEN** preview permanece indisponível com explicação explícita de que o suporte está em desenvolvimento, sem caracterizar a mídia comercial como proibida
