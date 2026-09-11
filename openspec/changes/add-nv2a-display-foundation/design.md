## Context

Bus, MMU, scheduler, machine, kernel HLE e desktop existem; `libs/gpu` é apenas placeholder. O marco inicia o caminho gráfico sem misturar apresentação host com emulação nem alegar NV2A completo.

## Goals / Non-Goals

**Goals:** PCI Type 0/BAR; dispositivo NV2A modular; pushbuffer bounded; operações RGBA 2D determinísticas; frame snapshot ABI e preview desktop.

**Non-Goals deste marco:** rasterização 3D completa, shaders, texturas avançadas, Vulkan/Metal/D3D, aceleração host, precisão cycle-exact ou execução funcional de jogos. Isso não altera a meta final de executar `.xbe`, `.iso` e `.xiso` fornecidos pelo usuário.

## Decisions

### Camadas e dependências

Criar `libs/pci` dependente de common+bus e implementar `libs/gpu` dependente de common+bus+memory+core. Machine compõe ambos. GPU não conhece machine, ABI, Rust ou React.

### Estado e registros

Separar register file, surface, pushbuffer decoder/processor e NV2A device em arquivos próprios. Registrar apenas offsets/bitmasks com proveniência pública. Unknown reads/writes geram diagnóstico conservador; nenhum array indexado diretamente por endereço hostil.

### PCI e MMIO

Config space usa BDF e acesso tipado. BAR programming ocorre em transação: validar sizing/alinhamento/range e só então substituir route no bus. Command register bloqueia BAR quando memory-space está desligado.

### Pushbuffer

Decoder converte words little-endian em packets imutáveis. Processor valida packet inteiro e budget antes de aplicar. Escopo inicial oferece bind surface, clear, fill rectangle e flip; nomes deixam claro que são subconjunto diagnóstico. Jumps, se incluídos, têm visited/budget guard.

### Surface e frame

Core mantém RGBA8 linear com dimensões/pitch limitados e sequence monotônica. Flip cria snapshot imutável/copy-on-request. ABI C 1.3 usa struct_size e two-call; Rust só copia/serializa DTO. Frontend usa Canvas/ImageData com cleanup e sem polling irrestrito. O preview diagnóstico é uma etapa incremental rumo à futura janela de jogo; controles gerais de execução só serão habilitados quando o pipeline correspondente existir e for validado objetivamente.

### Scheduling e IRQ

Submissões consomem custos determinísticos simplificados; conclusão e flip são eventos do scheduler. IRQ controller recebe uma fonte GPU narrow-interface. Reset cancela eventos por IDs e zera estado.

### Segurança e testes

Fixtures constroem config transactions e pushbuffers programaticamente. Testar malformed, overflow, overlap, budgets, atomicidade, reset, pixel golden pequeno e duas execuções idênticas. ASan/UBSan e limite ~500 linhas. Documentar Intel PCI e fontes NV2A públicas clean-room.

## Risks / Trade-offs

- Constantes NV2A imprecisas → escopo allowlisted, referências públicas e testes sem alegar hardware completo.
- Pixel transport caro → snapshots bounded; otimização futura sem ponteiros compartilhados.
- GPU vira god file → módulos register/surface/decoder/processor/device.
- UI alegar compatibilidade antes da hora → rótulo diagnóstico neste marco; futuramente promover a controles de jogo somente após critérios objetivos por título/formato.
- Eventos sobrevivem reset → IDs canceláveis e generation token.

## Migration Plan

1. PCI config/BAR e testes.
2. Surface/register file/device e reset.
3. Decoder/processor e métodos 2D.
4. Scheduler/IRQ/machine integration.
5. ABI 1.3, Rust e preview React.
6. CI, sanitizers, documentação, diário e auditoria legal.