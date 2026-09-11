## Purpose

Fornece namespace Xbox portátil e seguro sobre volumes sem espalhar detalhes do host nos subsistemas.

## ADDED Requirements

### Requirement: Mounts e caminhos Xbox
O VFS SHALL montar volumes por drive/device alias e normalizar `D:\path`, barras e componentes sem permitir escape, caminho host absoluto ou traversal.

#### Scenario: Traversal
- **WHEN** caminho contém `..` tentando sair do volume
- **THEN** resolução é rejeitada sem consultar filesystem host

### Requirement: Handles geracionais
Arquivos e diretórios abertos SHALL usar handles opacos geracionais, com tipo, posição e acesso; stale/double-close MUST falhar com status estruturado.

#### Scenario: Handle reutilizado
- **WHEN** slot fechado é reaberto
- **THEN** handle antigo não acessa o novo objeto

### Requirement: I/O read-only de disco
Volumes de mídia SHALL permitir open/read/seek/query/enumerate e rejeitar create/write/delete de forma explícita.

#### Scenario: Escrita em D:
- **WHEN** guest solicita acesso de escrita
- **THEN** retorna read-only sem modificar imagem

### Requirement: Isolamento por sessão
Cada MachineSession SHALL possuir namespace e tabela de handles independentes, com fechamento determinístico no teardown.

#### Scenario: Duas sessões
- **WHEN** mesmo arquivo é aberto em sessões distintas
- **THEN** offsets e lifecycle não são compartilhados
