## Why
A GPU atual só executa operações 2D diagnósticas. Títulos precisam de um subconjunto geral do pipeline 3D NV2A.

## What Changes
- Implementar estado 3D, vertex/index fetch, primitives, texturas e rasterizador software inicial.
- Expandir métodos pushbuffer com diagnóstico preciso.
- Adicionar testes golden sintéticos e budgets.

## Capabilities
### New Capabilities
- `nv2a-3d-reference`: Pipeline 3D software determinístico inicial.

### Modified Capabilities
- `nv2a-pushbuffer`: Métodos/classes 3D adicionais e relatório unsupported.

## Impact
Somente `libs/gpu/**`, testes GPU e docs/diário da lane; sem hacks por título.