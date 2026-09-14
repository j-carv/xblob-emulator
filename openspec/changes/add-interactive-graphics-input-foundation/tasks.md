## 1. Preparação
- [x] 1.1 Validar/checkpointar parent e child plans com contratos e proibição de hacks por título.
- [x] 1.2 Criar integration branch e lanes `nv2a-3d`/`usb-xid` no mesmo BASE_SHA, registrando ownership/task IDs.

## 2. Integração das lanes
- [x] 2.1 Validar ownership/diff/testes da GPU e integrar serialmente.
- [x] 2.2 Validar ownership/diff/testes de USB/XID e integrar serialmente.
- [x] 2.3 Adaptar contratos exclusivamente na integration lane e testar após cada merge.

## 3. Machine e diagnóstico
- [x] 3.1 Integrar GPU Advance/frame publication e USB input/IRQ aos slices da MachineSession.
- [x] 3.2 Implementar frame pacing/backpressure e input snapshots thread-safe com teardown bounded.
- [x] 3.3 Agregar unsupported GPU/USB por capability/count/context sem dados proprietários.
- [x] 3.4 Testar pause/resume/stop, input durante pause, frame sequence, IRQ e duas execuções.

## 4. ABI e desktop
- [x] 4.1 Evoluir ABI C para 1.6 preservando 1.0–1.5, com frame/input/config/metrics sized e bounded.
- [x] 4.2 Implementar Rust input adapter e frame polling/backpressure sem lógica de hardware.
- [x] 4.3 Implementar janela/painel React com frame contínuo, teclado/gamepad mapping, focus e controles acessíveis.
- [x] 4.4 Exibir diagnóstico geral e aviso experimental, sem código/texto específico para títulos.

## 5. Qualidade
- [x] 5.1 Executar CTest normal/sanitizers, format, clangd, C11 legacy e audits race/ownership/god files.
- [x] 5.2 Executar Rust fmt/clippy/test/build e npm lint/typecheck/test/build/Tauri CLI.
- [x] 5.3 Atualizar CI, README, ARCHITECTURE, docs/proveniência e diário.
- [x] 5.4 Validar parent/children strict e registrar checkpoint na branch de integração após gates (integração em main e remoção de worktrees ficam fora deste pedido).
