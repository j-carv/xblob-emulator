## Purpose
Renderiza primitives 3D iniciais de forma determinística e geral.

## ADDED Requirements

### Requirement: Estado e vertices
GPU SHALL suportar viewport/scissor, vertex buffers, strides/formats prioritários e indexed/non-indexed triangle lists com bounds checked.
#### Scenario: Vertex fora da memória
- **WHEN** fetch cruza range guest
- **THEN** draw falha antes de alterar render target

### Requirement: Rasterização
Rasterizador SHALL aplicar clipping/scissor, winding, barycentric interpolation, depth test/write e blend básico em RGBA8.
#### Scenario: Triângulos sobrepostos
- **WHEN** depth test está ativo
- **THEN** pixels seguem comparação e write mask configurados

### Requirement: Texturas
GPU SHALL suportar textura 2D linear/swizzled em formatos básicos allowlisted, addressing clamp/wrap e nearest sampling.
#### Scenario: Formato desconhecido
- **WHEN** texture format não suportado é bound
- **THEN** draw retorna unsupported-format com identificador

### Requirement: Pushbuffer 3D
Métodos suportados SHALL atualizar shadow state e draws SHALL validar snapshot completo antes do commit, com budgets.
#### Scenario: Método ausente
- **WHEN** class/method não existe
- **THEN** diagnóstico inclui class, method, subchannel e count bounded

### Requirement: Generalidade
Código MUST NOT consultar título, hash, nome de arquivo ou Title ID.
#### Scenario: Regressão
- **WHEN** comportamento é adicionado por feedback de jogo
- **THEN** fixture sintética genérica demonstra o método/hardware
