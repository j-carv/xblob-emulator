## Purpose

Monta e navega XDVDFS em imagens Xbox fornecidas pelo usuário com validação hostil e I/O bounded.

## ADDED Requirements

### Requirement: Detecção e montagem
O sistema SHALL localizar e validar volume descriptor XDVDFS nas variantes raw e trimmed suportadas, usando offsets/setores checked e sem carregar a imagem inteira.

#### Scenario: Volume válido
- **WHEN** descriptor, root sector e tamanhos são válidos
- **THEN** mount read-only é criado com metadata determinística

#### Scenario: Descriptor truncado
- **WHEN** qualquer campo aponta fora da fonte
- **THEN** mount falha sem leitura parcial fora de bounds

### Requirement: Árvore de diretórios
O parser SHALL percorrer entradas por índices/offsets validados, impondo limites de profundidade, nós e bytes de nome e detectando ciclos/duplicatas inválidas.

#### Scenario: Ciclo malicioso
- **WHEN** links da árvore revisitam entrada já ativa
- **THEN** retorna erro estrutural bounded sem recursão infinita

### Requirement: Caminhos e nomes
Lookup SHALL ser case-insensitive conforme semântica documentada, rejeitar separadores/componentes inválidos e preservar nome original para apresentação.

#### Scenario: Default XBE
- **WHEN** raiz contém `DEFAULT.XBE`
- **THEN** lookup de `default.xbe` retorna a mesma entrada

### Requirement: Leitura de arquivos
File reads SHALL validar offset+length, setores e tamanho lógico e retornar somente bytes pertencentes à entrada.

#### Scenario: Read no EOF
- **WHEN** leitura atravessa tamanho lógico
- **THEN** retorna apenas bytes restantes sem acessar setor posterior indevido
