## Purpose

Fornece memória convidada segura e determinística para os subsistemas emulados, isolando endereços de 32 bits da memória e do ABI do host.

## ADDED Requirements

### Requirement: Configuração de RAM física
O sistema SHALL criar RAM convidada inicializada em zero com capacidade explicitamente selecionada de 64 MiB ou 128 MiB e MUST rejeitar capacidades não suportadas.

#### Scenario: Máquina retail de 64 MiB
- **WHEN** uma instância de memória é criada no modo retail
- **THEN** ela expõe exatamente 64 MiB endereçáveis e todos os bytes iniciais são zero

#### Scenario: Capacidade inválida
- **WHEN** o consumidor solicita uma capacidade diferente de 64 MiB ou 128 MiB
- **THEN** a criação falha com erro estruturado sem alocar memória parcial

### Requirement: Acessos tipados little-endian
A memória SHALL permitir leitura e escrita de valores de 8, 16 e 32 bits em little-endian somente quando toda a faixa estiver mapeada e permitida.

#### Scenario: Escrita e leitura de 32 bits
- **WHEN** o valor `0x12345678` é escrito em uma região RAM gravável e lido novamente
- **THEN** os bytes armazenados são `78 56 34 12` e a leitura retorna `0x12345678`

#### Scenario: Acesso cruza limite
- **WHEN** uma leitura ou escrita cruza o fim de uma região
- **THEN** o acesso falha antes de ler ou modificar qualquer byte

### Requirement: Espaço de endereçamento de 32 bits
O sistema SHALL representar endereços convidados como valores de 32 bits e SHALL resolver somente mapeamentos explícitos, sem converter endereços convidados em ponteiros do host.

#### Scenario: Endereço não mapeado
- **WHEN** um subsistema acessa um endereço de 32 bits sem região correspondente
- **THEN** recebe uma falha `unmapped` determinística

#### Scenario: Mapeamentos sobrepostos
- **WHEN** o consumidor tenta registrar uma região que sobrepõe outra
- **THEN** o novo mapeamento é rejeitado e o mapa existente permanece intacto

### Requirement: Permissões de região
Cada mapeamento SHALL declarar permissões de leitura, escrita e execução, e todo acesso MUST validar a permissão correspondente antes de produzir efeito.

#### Scenario: Escrita em região somente leitura
- **WHEN** um valor é escrito em uma região sem permissão de escrita
- **THEN** a operação falha com violação de proteção e o conteúdo permanece inalterado

#### Scenario: Fetch em região não executável
- **WHEN** a CPU busca opcode em região sem permissão de execução
- **THEN** a busca falha com violação de execução

### Requirement: MMIO por contrato explícito
O espaço de endereçamento SHALL permitir mapear uma região MMIO por callbacks tipados, encaminhando endereço relativo, largura e valor sem expor ponteiros do host.

#### Scenario: Escrita em registrador MMIO
- **WHEN** ocorre uma escrita válida em uma região MMIO
- **THEN** o dispositivo recebe uma única chamada com offset, largura e valor corretos

#### Scenario: Dispositivo rejeita largura
- **WHEN** o dispositivo não suporta a largura solicitada
- **THEN** a falha estruturada do dispositivo é propagada sem fallback para RAM
