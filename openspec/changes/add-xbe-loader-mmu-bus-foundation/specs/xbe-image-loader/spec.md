## Purpose

Carrega imagens XBE sintéticas já validadas em memória convidada de forma transacional, preparando estado inicial sem executar kernel ou imports.

## ADDED Requirements

### Requirement: Carregamento transacional
O loader SHALL validar todas as faixas, endereços e sobreposições antes de modificar a máquina; qualquer falha MUST deixar memória e mapeamentos no estado anterior.

#### Scenario: Seção malformada tardia
- **WHEN** uma seção posterior possui faixa inválida
- **THEN** nenhuma seção anterior permanece carregada

### Requirement: Headers, seções e zero-fill
O loader SHALL copiar bytes de headers e seções, preencher com zero a diferença válida entre tamanho virtual e raw e rejeitar raw maior que limites declarados.

#### Scenario: BSS sintética
- **WHEN** seção possui tamanho virtual maior que raw
- **THEN** bytes restantes ficam zero sem ler além do arquivo

### Requirement: Permissões de seção
Flags XBE suportadas SHALL ser convertidas em permissões guest conservadoras; combinações desconhecidas ou incompatíveis MUST produzir erro suportado/estruturado.

#### Scenario: Seção de código
- **WHEN** seção sintética é executável e não gravável
- **THEN** fetch é permitido e escrita é rejeitada

### Requirement: Contexto inicial
O resultado SHALL informar entry point decodificado, base, stack provisória sintética e metadados necessários à sessão, sem resolver imports ou TLS não suportados silenciosamente.

#### Scenario: Recurso obrigatório não suportado
- **WHEN** imagem exige mecanismo fora do escopo declarado
- **THEN** preparação retorna unsupported em vez de iniciar execução parcial
