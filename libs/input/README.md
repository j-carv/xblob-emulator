# xblob Input & XID Subsystem (`libs/input`)

O subsistema de input fornece estruturas de domínio neutras para captura de controles e a implementação do periférico Xbox XID (`XidController`) sobre o contrato `UsbDevice`.

## Componentes

1. **`HostInputSnapshot`**:
   - Estrutura imutável e desacoplada de bibliotecas ou APIs da plataforma host.
   - Validação estrita de sequência monotônica (`sequence`), descartando snapshots defasados (stale) para evitar regressão temporal ou conflitos de concorrência.
   - Normalização e clamp de botões analógicos de pressão (0..255), gatilhos (0..255) e direcionais analógicos (-32768..32767).

2. **`XidGamepadReport`**:
   - Layout binário canônico de 20 bytes do controle Xbox Duke / Controller S.
   - Serialização e deserialização determinísticas em little-endian.

3. **`XidController`**:
   - Implementa a interface `xblob::usb::UsbDevice`.
   - Enumeração completa USB 1.1: Device Descriptor, Configuration Descriptor com endpoints de Interrupção IN (0x81) e OUT (0x02), e descritor de capacidades XID (tipo 0x42).
   - Allowlist estrita de comandos de controle (`GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION`, `GET_CAPABILITIES`, `SET_REPORT` para vibração dos motores de rumble). Comandos não suportados respondem com `STALL` imediato.
   - Suporte a hotplug determinístico (`Connect()` / `Disconnect()`) sem ponteiros pendentes ou callbacks órfãos.
