## Purpose

Fornece descoberta PCI e BARs mínimos para dispositivos convidados sem acoplamento ao host.

## ADDED Requirements

### Requirement: Configuração PCI
O sistema SHALL modelar bus/device/function e header Type 0 com IDs, command/status, class e BARs little-endian.

#### Scenario: Dispositivo presente
- **WHEN** configuração do dispositivo gráfico é lida por width/alignment suportados
- **THEN** IDs e classe documentados são retornados deterministicamente

#### Scenario: Função ausente
- **WHEN** BDF não registrado é lido
- **THEN** o valor all-ones apropriado é retornado sem fault host

### Requirement: BAR seguro
BAR sizing/programming SHALL validar tipo, alinhamento, overflow e colisão antes de alterar roteamento MMIO.

#### Scenario: BAR sobreposto
- **WHEN** guest programa range conflitante
- **THEN** alteração é rejeitada atomicamente e mapa anterior permanece ativo

### Requirement: Command gating
Memory-space enable SHALL controlar acesso aos BARs; capabilities não implementadas permanecem explicitamente ausentes.

#### Scenario: MMIO desabilitado
- **WHEN** command.memory_space está limpo
- **THEN** acesso ao BAR não alcança o dispositivo
