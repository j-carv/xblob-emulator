## Why

O xblob já executa programas diagnósticos IA-32 sintéticos com kernel HLE, mas não possui caminho gráfico. A próxima fundação deve conectar descoberta PCI, registros NV2A, consumo de pushbuffer e apresentação de framebuffer de forma determinística e clean-room antes de qualquer rasterização 3D ampla.

## What Changes

- Implementar configuração PCI mínima e roteamento de BARs para dispositivos convidados.
- Ativar `libs/gpu` com dispositivo NV2A inicial, registros MMIO allowlisted e estado observável.
- Implementar parser/processor defensivo de pushbuffer sintético com budgets e faults estruturados.
- Produzir framebuffer RGBA determinístico por operações 2D mínimas e apresentá-lo no desktop.
- Integrar GPU ao bus, scheduler, machine, ABI C e React sem expor ponteiros host ou permissões inseguras.
- Adicionar fixtures sintéticas, testes, CI, documentação de proveniência e diário.

## Capabilities

### New Capabilities
- `pci-bus-foundation`: Espaço de configuração PCI e BARs mínimos, determinísticos e seguros.
- `nv2a-device-foundation`: Registros e lifecycle inicial do dispositivo gráfico clean-room.
- `nv2a-pushbuffer`: Decodificação limitada e defensiva de comandos pushbuffer sintéticos.
- `framebuffer-presentation`: Surface RGBA versionada e apresentação segura no frontend.

### Modified Capabilities
- `machine-session`: Coordenar GPU e eventos sem absorver lógica gráfica.
- `diagnostic-execution`: Expor frames de workloads sintéticos neste marco, mantendo arquitetura evolutiva para futuramente executar mídia fornecida pelo usuário.

## Impact

Cria implementação real em `libs/gpu`, amplia bus/machine/C ABI/Tauri/React e CI. Este marco ainda não implementa shaders, rasterização 3D completa, precisão cycle-exact, Vulkan/Metal/D3D ou execução funcional de jogos. A meta do produto permanece carregar e jogar `.xbe`, `.iso` e `.xiso` fornecidos legalmente pelo usuário; o repositório jamais distribuirá mídias, BIOS, chaves, firmware ou SDK proprietário.