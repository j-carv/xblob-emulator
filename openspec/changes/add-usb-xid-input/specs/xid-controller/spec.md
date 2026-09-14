## Purpose
Representa controle Xbox geral e traduz snapshots normalizados em reports XID.

## ADDED Requirements

### Requirement: Descriptor e enumeração
Dispositivo SHALL responder a requests USB/XID allowlisted com descriptors sintéticos compatíveis e rejeitar requests desconhecidos explicitamente.
#### Scenario: Get descriptor
- **WHEN** host controller solicita descriptor suportado
- **THEN** resposta bounded contém tamanho/tipo consistentes

### Requirement: Input report
Report SHALL incluir digital buttons, sticks signed e triggers unsigned com layout/endianness explícitos e clamp.
#### Scenario: Stick extremo
- **WHEN** snapshot recebe valor fora da faixa
- **THEN** report aplica clamp determinístico

### Requirement: Snapshot thread-safe
Backend SHALL aceitar estado normalizado por sequence, ignorar stale snapshots e produzir report imutável sem API host no domínio.
#### Scenario: Snapshot antigo
- **WHEN** sequence menor chega após nova
- **THEN** estado atual não retrocede

### Requirement: Hotplug
Connect/disconnect SHALL alterar port state e transfers pending de modo determinístico e seguro.
#### Scenario: Disconnect
- **WHEN** controle desconecta
- **THEN** novos polls recebem estado apropriado sem dangling callback

### Requirement: Generalidade
Mapping guest MUST NOT depender de jogo/Title ID; feedback real gera fixtures genéricas.
#### Scenario: Primeiro jogo
- **WHEN** Battlefront II é testado
- **THEN** qualquer correção representa comportamento geral XID/USB
