# Plataforma e Abstrações do Host (libs/platform)

*(Subsistema planejado para marcos futuros)*

## Responsabilidade
Encapsular primitivas do sistema operacional host (gerenciamento de janelas, backends gráficos como Metal/Vulkan/DirectX, saída de áudio PCM, relógio de alta precisão e primitivas de sincronismo multithread).

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`.
- **Regra estrita**: Código específico de cada sistema (Windows SDK, macOS Cocoa/Metal, Linux POSIX/Wayland) fica estritamente contido neste diretório e nunca vaza para o domínio de emulação.
