## Purpose

Define uma fronteira binária estável e segura para que shells e outras linguagens consumam capacidades do núcleo C++ sem compartilhar ABI C++ ou ownership implícito.

## ADDED Requirements

### Requirement: ABI C versionada
A biblioteca SHALL exportar uma API C com versão major/minor consultável, convenção de chamada portátil e símbolos públicos limitados ao contrato documentado.

#### Scenario: Consulta de versão
- **WHEN** um consumidor compatível carrega a biblioteca
- **THEN** ele obtém versão da ABI e versão do produto sem criar contexto ou acessar tipos C++

### Requirement: Fronteira sem exceções e tipos C++
Toda função exportada MUST usar somente tipos C, inteiros de largura fixa, structs versionadas ou handles opacos e MUST NOT permitir que exceções atravessem a fronteira.

#### Scenario: Erro interno inesperado
- **WHEN** uma operação interna falha inesperadamente
- **THEN** a ABI retorna status de erro e mensagem UTF-8 recuperável sem unwinding no consumidor

### Requirement: Ownership explícito
Objetos e strings produzidos pela ABI SHALL possuir ciclo de vida documentado e função de destruição idempotente ou regra inequívoca de buffer fornecido pelo chamador.

#### Scenario: Resultado de inspeção liberado
- **WHEN** o consumidor termina de ler um resultado válido
- **THEN** ele pode liberar todos os recursos pela função correspondente sem usar allocator próprio sobre memória da biblioteca

### Requirement: Inspeção de mídia pela ABI
A ABI SHALL permitir inspecionar um caminho UTF-8 e consultar tipo, tamanho, metadados básicos e erro estruturado equivalentes ao núcleo, sem executar conteúdo convidado.

#### Scenario: XBE sintético válido
- **WHEN** um consumidor inspeciona fixture XBE sintética válida
- **THEN** recebe tipo XBE, tamanho e metadados básicos coerentes com `MediaInspector`

#### Scenario: Caminho inválido
- **WHEN** o consumidor fornece caminho inexistente ou UTF-8 inválido
- **THEN** recebe categoria e mensagem de erro determinísticas sem handle parcialmente válido

### Requirement: Compatibilidade estrutural
Structs públicas SHALL conter campo de tamanho/versão quando extensíveis, e versões minor compatíveis MUST preservar campos e semântica existentes.

#### Scenario: Consumidor com struct menor
- **WHEN** um consumidor informa tamanho suportado menor que a revisão atual, mas compatível
- **THEN** a biblioteca escreve somente dentro do tamanho informado e retorna os campos disponíveis
