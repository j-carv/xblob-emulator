## Purpose

Fornece tradução virtual IA-32 inicial, segura e determinística para separar endereços usados pela CPU dos mapeamentos físicos e MMIO da máquina.

## ADDED Requirements

### Requirement: Paginação de 4 KiB
O sistema SHALL traduzir endereços lineares de 32 bits por diretório e tabelas de páginas de 4 KiB quando paginação estiver ativa, lendo entradas em little-endian pela memória física.

#### Scenario: Tradução presente
- **WHEN** PDE e PTE válidos apontam para uma página física mapeada
- **THEN** o endereço linear é traduzido preservando o offset de 12 bits

### Requirement: Permissões e faults
A tradução SHALL validar bits present, read/write e user/supervisor conforme tipo de acesso e nível configurado, retornando fault com endereço e código de causa.

#### Scenario: Escrita em página somente leitura
- **WHEN** ocorre escrita sem permissão efetiva
- **THEN** nenhuma memória é alterada e o fault identifica proteção de escrita

#### Scenario: Página ausente
- **WHEN** PDE ou PTE não está presente
- **THEN** o fault distingue ausência de página de violação de proteção

### Requirement: Paginação desativada
Com paginação desativada, o sistema SHALL tratar endereço linear como físico sem truncamento ou ponteiro host.

#### Scenario: Identidade sem paginação
- **WHEN** a CPU acessa endereço linear mapeado com paginação desligada
- **THEN** o acesso usa o mesmo endereço físico e ainda respeita permissões da região

### Requirement: Invalidação explícita
Alterações nas estruturas de página SHALL ser observáveis após invalidação explícita, e qualquer cache de tradução MUST preservar comportamento determinístico.

#### Scenario: PTE remapeada
- **WHEN** uma PTE muda e sua tradução é invalidada
- **THEN** o próximo acesso usa a nova página física
