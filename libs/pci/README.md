# Barramento e Configuração PCI (libs/pci)

## Responsabilidade
Implementar o modelo mínimo do barramento PCI (Peripheral Component Interconnect), abstraindo:
- Endereçamento BDF (Bus/Device/Function).
- Cabeçalho de configuração Type 0 com acessos 8, 16 e 32-bit little-endian.
- Suporte a Base Address Registers (BARs) com protocolo padrão de sizing (`0xFFFFFFFF`), alinhamento e detecção de colisões.
- Gating de MMIO através do bit `memory_space` no registrador `command`.
- Registro determinístico e enumeração ordenada de dispositivos.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/bus`.
- **Não permitido**: Acoplamento com detalhes de host, SO ou drivers proprietários.

## Proveniência Clean-Room
Implementado estritamente com base em especificações públicas do padrão PCI Local Bus Specification (Revisão 2.2/2.3) e literatura pública sobre a arquitetura do console original, sem inclusão de código ou cabeçalhos proprietários.
