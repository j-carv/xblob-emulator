## Why

O Marco 7 permite executar mídia real de forma bounded e diagnosticar o primeiro bloqueio, mas títulos ainda não têm pipeline 3D nem controle guest. Este marco conecta uma fundação NV2A 3D e USB/XID ao loop experimental para obter frames e interação iniciais.

Star Wars Battlefront II (2005) será somente um título local de validação pelo usuário. O design MUST permanecer geral, baseado em hardware/APIs do Xbox e sem hacks, patches, fingerprints ou código específico por jogo.

## What Changes

- Congelar contratos de GPU command submission/frame e input snapshot/USB antes de duas lanes paralelas.
- Integrar as child changes `expand-nv2a-3d-pipeline` e `add-usb-xid-input`.
- Conectar GPU/input, scheduler, IRQ e execução experimental em `MachineSession`.
- Evoluir ABI C, Tauri e React para janela de execução, frames contínuos e configuração de controle.
- Produzir diagnóstico de métodos/formatos/USB requests não suportados para evolução orientada por compatibilidade geral.
- Manter mídia do usuário local e testes do repositório exclusivamente sintéticos.

## Capabilities

### New Capabilities
- `interactive-emulation-session`: Sessão experimental com frame pacing, input e lifecycle responsivo.
- `title-agnostic-compatibility`: Telemetria por capacidade sem caminhos específicos por título.

### Modified Capabilities
- `framebuffer-presentation`: Apresentar sequência de frames 3D bounded.
- `experimental-title-execution`: Integrar input e eventos GPU ao worker.

## Impact

Integra GPU 3D e USB/XID em machine, ABI C 1.6 e desktop. Não garante que Battlefront II ou qualquer jogo seja jogável; o título apenas fornecerá feedback local para identificar capacidades gerais ausentes.