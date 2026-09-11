# Fundações da GPU NV2A e Framebuffer (libs/gpu)

## Responsabilidade

Implementar a fundação gráfica da GPU NV2A (NVIDIA GeForce 3 derivativo), fornecendo:
- **Banco de Registradores com Allowlist**: Mapeamento seguro e verificação de acessos de leitura/escrita para faixas conhecidas (`PMC`, `PBUS`, `PFIFO`, `PGRAPH`, `PCRTC`). Acessos a endereços fora da allowlist são rejeitados de forma segura e determinística.
- **Superfície Linear de Display (RGBA8)**: Abstração de buffer de quadro puro em memória de host (`GpuSurface`), com contagem de bytes estritamente delimitada (`width * height * 4`), checagem de coordenadas para operações 2D e numeração de versão de snapshot (`snapshot_version`).
- **Decodificador e Processador de Pushbuffer**:
  - `Nv2aPushbufferDecoder`: Decodifica fluxos de comandos de pushbuffer estruturados em métodos, subcanais e tipos de pacotes (Method / Non-Incrementing).
  - `Nv2aPushbufferProcessor`: Executa operações gráficas fundamentais 2D (`BIND_SURFACE`, `CLEAR`, `FILL_RECT`, `FLIP_SURFACE`) de forma puramente determinística em CPU, sem dependência de drivers do host ou aceleração proprietária.
- **Orçamentos Rígidos de Execução**: Prevenção de travamentos e loops infinitos através de limites configuráveis de palavras decodificadas, pacotes processados, métodos despachados e ciclos virtuais.
- **Interrupções de GPU**: Sinalização determinística de eventos de quadro (`GpuInterruptSource`) integrada ao barramento e escalonador da máquina convidada.

## Fronteiras e Dependências

- **Dependências permitidas**: `libs/common`, `libs/bus`, `libs/pci`.
- **Não permitido**: Dependências diretas de drivers de vídeo proprietários, DirectX, OpenGL, Vulkan ou Metal. A camada opera estritamente em representação de pixels em memória de host, permitindo exportação portável via ABI C e exibição em frontends desacoplados (como o canvas do desktop).

## Proveniência Clean-Room

Implementado a partir de especificações públicas e literatura aberta da arquitetura NV2A e do barramento gráfico do console, sem inclusão de headers, códigos ou firmwares proprietários. Fixtures automatizadas utilizam comandos e padrões geométricos sintéticos gerados programmaticamente.
