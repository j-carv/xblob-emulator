## Context

A máquina possui execução assíncrona, framebuffer 2D, PCI/NV2A inicial e ABI 1.5. Não há rasterização 3D nem USB/XID. Duas lanes independentes podem implementar os núcleos, deixando hotspots à integração.

## Goals / Non-Goals

**Goals:** primitives 3D iniciais, texturas básicas, depth/blend; USB/XID e backend host abstrato; frame/input loop desktop; diagnóstico geral.

**Non-Goals deste marco:** NV2A completo, shaders avançados, áudio, force feedback, network, compatibilidade garantida ou hacks para Battlefront II.

## Decisions

### Contratos congelados

GPU lane produz `SubmitPushbuffer`, `Advance(cycles)`, immutable `FrameSnapshot` e `GpuUnsupportedMethod`. Input lane produz `HostInputSnapshot`, `XidReport`, `UsbTransferResult` e IRQ events. Integration lane adapta esses contratos sem exigir dependência entre lanes.

### Lanes

`nv2a-3d` owns `libs/gpu/**`, testes GPU e README/diário próprios. `usb-xid` cria/owns `libs/input/**`, `libs/usb/**`, testes correspondentes e docs/diário próprios. Hotspots root CMake, bus/machine, ABI, apps, lockfiles e docs globais ficam proibidos.

### Renderizador de referência

Implementar rasterizador software determinístico no core para triangles, viewport/scissor, depth, basic blend e nearest texture. Separar command state, vertex fetch, texture decode e raster. Backend acelerado fica futuro; referência fornece correção e testes golden.

### USB/XID

Modelar OHCI mínimo por registros/ED/TD estritamente necessários e um dispositivo XID generalista. Backend host injeta estado normalizado; nenhuma API de plataforma entra em USB/domain. Teclado/gamepad mapping ocorre na shell/UI integration lane.

### Integração

Machine recebe devices e slices; GPU IRQ e USB IRQ usam controller existente. ABI C 1.6 copia frames/input/config por structs sized. Rust coleta eventos host e envia snapshots; React usa requestAnimationFrame apenas para apresentação, com backpressure por sequence.

### Compatibilidade geral

Nenhum código consulta Title ID/hash/nome. Feedback do jogo escolhido vira issue por capacidade e fixture sintética mínima. Relatórios agregam eventos e redigem path/conteúdo.

## Risks / Trade-offs

- Raster software lento → limites e referência correta antes de aceleração.
- Métodos NV2A complexos → subset documentado e unsupported preciso.
- USB OHCI amplo → transfer/control subset incremental.
- Conflitos CMake/integration → ownership estrito e merges seriais.

## Migration Plan

1. Checkpointar planos e criar integration branch/worktrees.
2. Executar GPU e USB/XID em paralelo.
3. Validar ownership e integrar serialmente.
4. Integration lane conecta machine/ABI/desktop.
5. Gates completos, diário, merge main e cleanup.