## 1. Preparação
- [ ] 1.1 Validar/checkpointar parent e child plans com contratos e proibição de hacks por título.
- [ ] 1.2 Criar integration branch e lanes `nv2a-3d`/`usb-xid` no mesmo BASE_SHA, registrando ownership/task IDs.

## 2. Integração das lanes
- [ ] 2.1 Validar ownership/diff/testes da GPU e integrar serialmente.
- [ ] 2.2 Validar ownership/diff/testes de USB/XID e integrar serialmente.
- [ ] 2.3 Adaptar contratos exclusivamente na integration lane e testar após cada merge.

## 3. Machine e diagnóstico
- [ ] 3.1 Integrar GPU Advance/frame publication e USB input/IRQ aos slices da MachineSession.
- [ ] 3.2 Implementar frame pacing/backpressure e input snapshots thread-safe com teardown bounded.
- [ ] 3.3 Agregar unsupported GPU/USB por capability/count/context sem dados proprietários.
- [ ] 3.4 Testar pause/resume/stop, input durante pause, frame sequence, IRQ e duas execuções.

## 4. ABI e desktop
- [ ] 4.1 Evoluir ABI C para 1.6 preservando 1.0–1.5, com frame/input/config/metrics sized e bounded.
- [ ] 4.2 Implementar Rust input adapter e frame polling/backpressure sem lógica de hardware.
- [ ] 4.3 Implementar janela/painel React com frame contínuo, teclado/gamepad mapping, focus e controles acessíveis.
- [ ] 4.4 Exibir diagnóstico geral e aviso experimental, sem código/texto específico para Battlefront II.

## 5. Qualidade
- [ ] 5.1 Executar CTest normal/sanitizers, format, clangd, C11 legacy e audits race/ownership/god files.
- [ ] 5.2 Executar Rust fmt/clippy/test/build e npm lint/typecheck/test/build/Tauri CLI.
- [ ] 5.3 Atualizar CI, README, ARCHITECTURE, docs/proveniência e diário.
- [ ] 5.4 Validar parent/children strict, integrar main e remover worktrees após gates.