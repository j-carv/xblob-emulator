## 1. PCI mínimo
- [x] 1.1 Criar `xblob_pci` com BDF, Type 0 config header e acessos 8/16/32-bit little-endian; testar alignment, present/absent e bounds.
- [x] 1.2 Implementar registry PCI sem duplicatas e IDs/classe do dispositivo gráfico documentados; testar enumeração determinística.
- [x] 1.3 Implementar BAR sizing/programming transacional, alinhamento, overflow e collision checks; testar rollback.
- [x] 1.4 Integrar command.memory_space e routes BAR ao bus sem ciclos de dependência; testar enable/disable/remap.

## 2. Estado NV2A e surfaces
- [x] 2.1 Ativar `xblob_gpu` e separar register file, surface, device e tipos; verificar dependency graph e arquivos <500 linhas.
- [x] 2.2 Implementar register allowlist com widths/masks/read-only/W1C conforme escopo documentado; testar unknown/misaligned.
- [x] 2.3 Implementar RGBA8 surface com size/pitch/overflow limits e pixel operations checked; testar dimensões hostis e golden pixels.
- [x] 2.4 Implementar lifecycle/reset completo cancelando eventos e zerando fault/IRQ/frames; comparar com instância nova.

## 3. Pushbuffer
- [x] 3.1 Implementar fetch/decoder little-endian de packets incrementing/non-incrementing com ranges e contagens checked.
- [x] 3.2 Implementar budgets de words/packets/methods/cycles e proteção contra loops/jumps; testar exhaustion sem hang.
- [x] 3.3 Implementar bind surface, clear, fill-rect e flip allowlisted com validação/commit por packet.
- [x] 3.4 Testar truncamento, opcode/method unknown, rect/pitch inválido, packet sem efeitos parciais e sequência válida.

## 4. Scheduler, IRQ e machine
- [x] 4.1 Agendar submission/completion/flip por ciclo e sequence estável com custos explicitamente aproximados.
- [x] 4.2 Integrar status/mask e IRQ GPU à interface de interrupção existente; testar disabled/enabled/ack.
- [x] 4.3 Compor PCI/GPU em MachineSession mantendo delegação e lifecycle; testar duas sessões isoladas.
- [x] 4.4 Criar workload E2E sintético CPU→MMIO/pushbuffer→GPU→frame e comparar duas execuções byte a byte.

## 5. ABI e desktop
- [x] 5.1 Evoluir ABI C para 1.3 com capability e frame metadata/snapshot bounded, preservando consumidores 1.0–1.2 e C11.
- [x] 5.2 Implementar wrapper Rust RAII/IPC de frame sem lógica GPU e com buffers/dimensões limitados; testar erros e cleanup.
- [x] 5.3 Implementar preview React acessível via Canvas/ImageData, aspect ratio, estados e cleanup; testar axe e mock/real bridge.
- [x] 5.4 Manter eligibility diagnóstica e budgets neste marco, sem expor Play antes do pipeline funcional; testar mídia ainda não suportada e preservar contratos para futura execução de mídia do usuário.

## 6. Qualidade e documentação
- [x] 6.1 Atualizar CMake install/export e CI macOS/Linux/Windows para PCI/GPU sem duplicate-link warnings.
- [x] 6.2 Executar CTest normal e ASan+UBSan, warnings-as-errors, format e clangd; corrigir malformed/overflow/leaks.
- [x] 6.3 Executar npm ci/lint/typecheck/test/build e `npm run tauri -- --version`; cargo fmt/clippy/test/build locked.
- [x] 6.4 Auditar CSP, ownership, budgets, arquivos >500 linhas, dependency graph, generated artifacts e conteúdo proprietário.
- [x] 6.5 Atualizar README, ARCHITECTURE, docs PCI/NV2A/proveniência e diário com estado/limitações honestos.
- [x] 6.6 Validar `add-nv2a-display-foundation --strict` e marcar somente critérios reproduzíveis.
- [x] 6.7 Atualizar `AGENTS.md`, README, ARCHITECTURE e desktop docs para distinguir claramente o estado atual da meta final: o emulador deverá executar `.xbe`, `.iso` e `.xiso` próprios do usuário, enquanto o repositório e testes jamais incluem conteúdo proprietário.