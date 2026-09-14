# Diário de Bordo - 2026-09-14: Implementação da Fundação USB OHCI e Input XID (`add-usb-xid-input`)

## Contexto e Escopo

Esta sessão realizou a implementação integral e isolada da child change `add-usb-xid-input` dentro da lane dedicada de USB/Input. A meta principal é fornecer o suporte determinístico ao host controller USB OHCI mínimo e ao periférico Xbox XID (gamepad Duke / Controller S), mantendo desacoplamento total de APIs do host e sem introduzir hacks por título.

## Arquitetura Implementada

### 1. Host Controller OHCI Mínimo (`libs/usb`)
- **Registradores e Controle Operacional**:
  - Implementação completa dos registradores MMIO OHCI (`HcRevision`, `HcControl`, `HcCommandStatus`, `HcInterruptStatus`, `HcInterruptEnable`, `HcInterruptDisable`, `HcHCCA`, `HcPeriodCurrentED`, `HcControlHeadED`, `HcBulkHeadED`, `HcDoneHead`, `HcFmInterval`, `HcFmNumber`, `HcPeriodicStart`, `HcRhDescriptorA`, `HcRhPortStatus[0..3]`).
  - Semântica estrita de W1C (Write-1-to-Clear) para interrupções (`HcInterruptStatus`) e flags de mudança de estado nas portas do Root Hub.
  - Master Interrupt Enable (`MIE`, bit 31) com verificação rigorosa para asserção de IRQ.
- **Motor de Travessia Delimitada (`OhciTraversalEngine`)**:
  - Validação estrita de alinhamento em 16 bytes para Endpoint Descriptors (ED) e General Transfer Descriptors (TD).
  - Verificação de limites de memória física guest antes de qualquer leitura ou escrita.
  - Prevenção ativa contra laços infinitos e grafos malformados com orçamentos estritos (`kMaxEdTraversalBudget = 64`, `kMaxTdTraversalBudget = 128`) e rastreamento de nós visitados.
  - Commit transacional: se ocorrer falta de memória ou erro durante o processamento de um pacote, o estado guest é preservado sem mutações parciais ou corrupção de memória.

### 2. Dispositivo XID e Input do Usuário (`libs/input`)
- **`HostInputSnapshot`**:
  - Estrutura neutra e desacoplada da plataforma host para transporte de snapshots de controle.
  - Validação estrita de sequência monotônica (`sequence`), descartando snapshots com atraso ou duplicados para prevenir regressão temporal.
  - Clamping seguro e normalização de gatilhos (0..255), botões analógicos de pressão (0..255) e direcionais (-32768..32767).
- **`XidGamepadReport`**:
  - Empacotamento canônico de 20 bytes do gamepad Xbox original com serialização/deserialização little-endian determinística.
- **`XidController`**:
  - Implementação completa da interface `xblob::usb::UsbDevice`.
  - Descritores sintéticos clean-room: Device Descriptor (USB 1.10, VID 0x045E, PID 0x0289), Configuration Descriptor (32 bytes com endpoints de interrupção IN 0x81 e OUT 0x02) e Descritor de Recursos XID (tipo 0x42).
  - Allowlist estrita de requisições de controle (`GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION`, `GET_CAPABILITIES`, `SET_REPORT` para controle de vibração dos motores de rumble). Requisições não permitidas sofrem STALL imediato.
  - Suporte determinístico a hotplug (`Connect` e `Disconnect`) sem ponteiros órfãos ou vazamento de recursos.

## Testes e Validação

- **Conjunto de Testes Sintéticos Clean-Room**:
  - `tests/unit/test_usb.cpp`: 8 testes cobrindo defaults de reset, HostControllerReset, IRQ W1C e MIE, status de portas e resets, detecção de loops em ED e TD cíclicos, descritores desalinhados, flags Skip e Halted, e commit transacional em falhas de memória.
  - `tests/unit/test_input.cpp`: 6 testes cobrindo validação monotônica de sequência de snapshots, serialização/clamp de reports XID de 20 bytes, allowlist de descritores e requisições de controle, hotplug de desconexão/reconexão, determinismo exato em múltiplas execuções, e integração ponta-a-ponta OHCI + XID.
- **Sanitizadores e Estilo**:
  - Compilação e execução completas sob `-Wall -Wextra -Werror -std=c++20`.
  - Verificação com AddressSanitizer (`ASan`) e UndefinedBehaviorSanitizer (`UBSan`), reportando 0 erros e 0 vazamentos.
  - Aplicação de formatação automatizada com `clang-format`.
  - Todas as unidades mantidas com contagem estrita de linhas inferior a 500 linhas por arquivo.
