## Why

Após memória, scheduler, CPU sintética e UI de inspeção, o próximo passo coerente é conectar executáveis analisados a uma máquina convidada estruturada. Esta change introduz paginação IA-32 inicial, barramento e loader XBE sintético defensivo sem confundir preparação de memória com suporte a kernel ou jogos.

## What Changes

- Implementar tradução virtual IA-32 inicial com diretório/tabelas de páginas de 4 KiB, permissões e faults determinísticos.
- Introduzir `libs/bus` com mapa MMIO e dispositivos mínimos testáveis, sem dependências de host.
- Implementar loader XBE que valida novamente metadados, mapeia headers/seções, aplica zero-fill e permissões e produz contexto inicial sem executar imports/kernel.
- Criar sessão de máquina headless que agrega ownership de RAM, address space, bus, scheduler e CPU sem se tornar god object.
- Expor pela ABI C capacidades e diagnóstico da preparação sintética, mantendo a UI honesta e sem botão Play.
- Adicionar testes sintéticos de paginação, loader, rollback, MMIO e integração determinística.
- Manter CMake/CI, sanitizers, frontend/Rust, documentação e diário consistentes.

## Capabilities

### New Capabilities

- `x86-virtual-memory`: Tradução inicial de endereços virtuais IA-32 por páginas de 4 KiB e faults estruturados.
- `guest-bus`: Roteamento determinístico de MMIO e registro de dispositivos convidados.
- `xbe-image-loader`: Carregamento transacional de imagens XBE sintéticas validadas para memória convidada.
- `machine-session`: Composição e lifecycle de uma máquina headless preparada, pausada, faulted ou encerrada.

### Modified Capabilities

_Nenhuma._

## Impact

Ativa `libs/bus` e novos módulos de memória/loader/sessão, amplia CPU e ABI C e adiciona integração opcional de diagnóstico à UI. Não implementa kernel HLE, imports reais, interrupções completas, GPU, áudio, BIOS ou execução de jogos.