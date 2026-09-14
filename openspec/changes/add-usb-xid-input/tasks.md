## 1. USB OHCI
- [x] 1.1 Criar `xblob_usb` modular com register file, types e UsbDevice contract.
- [x] 1.2 Implementar reset/control/status/frame/interrupt registers e W1C masks.
- [x] 1.3 Implementar ED/TD traversal bounded com cycle/alignment/range checks.
- [x] 1.4 Implementar control e interrupt transfers mínimos, completion e IRQ.

## 2. XID e input
- [x] 2.1 Criar `xblob_input` com HostInputSnapshot e sequence validation.
- [x] 2.2 Implementar XID descriptors/control requests allowlisted.
- [x] 2.3 Implementar reports buttons/sticks/triggers com endian/clamp.
- [x] 2.4 Implementar connect/disconnect e port state sem callbacks órfãos.
- [x] 2.5 Testar stale snapshots, hotplug, polling e duas execuções.

## 3. Qualidade
- [x] 3.1 Criar malformed ED/TD/request fixtures e testar budgets/atomicidade.
- [x] 3.2 Executar testes USB/input normal/sanitizers, format e clangd.
- [x] 3.3 Verificar arquivos <500 linhas, dependency graph e clean-room.
- [x] 3.4 Atualizar READMEs/diário, validar strict/ownership e commit focado.