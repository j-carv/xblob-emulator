# Barramento do Sistema Convidado (libs/bus)

Este subsistema fornece a infraestrutura de barramento desacoplado para dispositivos virtuais e mapeamento de periféricos do guest.

## Estado Atual (Marco 3: Ativo)
- `BusDevice`: Interface abstrata para dispositivos conectados ao barramento convidado, com suporte a leituras e escritas de 8, 16 e 32 bits.
- `Bus`: Barramento determinístico com registro estático de dispositivos em faixas de endereçamento exclusivas (sem sobreposições).
- `SyntheticRegisterBank`: Implementação sintética de banco de registradores para testes unitários e periféricos virtuais mínimos.

## Limites Arquiteturais e Legais
- Este módulo implementa exclusivamente a camada abstrata de roteamento de barramento.
- Nenhum código proprietário de chipsets MCPX, pontes PCI ou controladores de hardware comercial está contido neste módulo.
- Dispositivos conectados utilizam unicamente protocolos e registradores sintéticos documentados publicamente.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`.
- Proibido depender de `libs/cpu`, `libs/core`, `libs/formats`, `libs/loader` ou `libs/machine`.
