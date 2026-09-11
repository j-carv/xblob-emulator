## Purpose

Disponibiliza operações HLE clean-room de arquivo e diretório sobre o VFS para código guest.

## ADDED Requirements

### Requirement: Contrato de arquivo HLE
O kernel SHALL implementar subconjunto documentado de open/create, read, seek, query, directory enumeration e close, convertendo argumentos guest checked para VFS.

#### Scenario: Leitura válida
- **WHEN** guest fornece handle e buffer mapeado suficientes
- **THEN** bytes são copiados, posição avança e status/information são retornados em 32 bits

### Requirement: Atomicidade de buffers
Validação de todos os argumentos e ranges guest SHALL ocorrer antes de alterar posição/handles ou copiar resultado.

#### Scenario: Buffer cruza página inválida
- **WHEN** parte do destino não é gravável
- **THEN** posição e memória guest permanecem inalteradas

### Requirement: Semântica assíncrona explícita
Neste marco, operações SHALL concluir sincronamente ou retornar unsupported; MUST NOT fingir pending/completion não implementado.

#### Scenario: Pedido async
- **WHEN** parâmetros exigem I/O assíncrono ainda não suportado
- **THEN** status explícito é retornado sem callback perdido

### Requirement: Status determinísticos
Erros VFS SHALL mapear para status guest estáveis para not-found, invalid-handle, access-denied, end-of-file, invalid-parameter e unsupported.

#### Scenario: EOF
- **WHEN** leitura inicia no final
- **THEN** retorna sucesso/EOF documentado com zero bytes sem fault host
