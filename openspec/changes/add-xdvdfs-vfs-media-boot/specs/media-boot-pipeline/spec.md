## Purpose

Unifica preparação de executável direto e contido em imagem, preservando ownership e diagnóstico honesto.

## ADDED Requirements

### Requirement: Fontes suportadas
O pipeline SHALL aceitar XBE direto e ISO/XISO XDVDFS, detectar tipo por conteúdo, montar imagem quando necessário e localizar `default.xbe` case-insensitivamente.

#### Scenario: Boot por XISO
- **WHEN** imagem válida contém default.xbe válido
- **THEN** loader recebe uma ByteSource bounded dessa entrada e prepara sessão

### Requirement: Sem extração insegura
XBE contido SHALL ser apresentado ao loader por view/stream bounded, sem gravar arquivo temporário por padrão ou confiar na extensão.

#### Scenario: Extensão enganosa
- **WHEN** arquivo `.iso` contém XBE direto válido
- **THEN** conteúdo determina o pipeline, não o sufixo

### Requirement: Diagnóstico por estágio
Falhas SHALL identificar detect, mount, lookup, parse, plan ou apply e manter sessão anterior intacta.

#### Scenario: Default ausente
- **WHEN** volume monta mas não contém default.xbe
- **THEN** resultado reporta lookup/not-found sem criar sessão preparada

### Requirement: Mídia local do usuário
O pipeline MUST operar localmente sem upload/telemetria e MUST NOT exigir que mídia, chaves ou dados proprietários sejam copiados ao repositório.

#### Scenario: Preparação local
- **WHEN** usuário escolhe mídia
- **THEN** bytes permanecem no dispositivo e somente dados necessários são lidos
