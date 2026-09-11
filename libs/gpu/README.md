# Emulação da GPU NV2A (libs/gpu)

*(Subsistema planejado para marcos futuros)*

## Responsabilidade
Implementar o pipeline gráfico e registradores da GPU NV2A personalizada da NVIDIA: processador de comandos (PushBuffers), transformações de vértices de função fixa, decodificação e execução de vertex shaders e pixel shaders, e rasterização.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/memory`, `libs/platform`.
