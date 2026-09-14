## 1. Preparação paralela
- [x] 1.1 Validar e checkpointar parent e child changes com contratos/ownership congelados.
- [x] 1.2 Criar `integration/experimental-title-execution` e lanes `cpu-execution` e `kernel-services` do mesmo BASE_SHA.
- [x] 1.3 Registrar ownership, denied hotspots, child change e task IDs no manifesto.

## 2. Integração das lanes
- [x] 2.1 Validar ownership/diff/testes da lane CPU e integrar serialmente sem force.
- [x] 2.2 Validar ownership/diff/testes da lane kernel e integrar serialmente sem force.
- [x] 2.3 Resolver apenas na integration lane qualquer adaptação de contrato e executar CTest afetado após cada merge.

## 3. Loop de execução
- [x] 3.1 Implementar command queue e ownership thread-safe da sessão com start/pause/resume/step/stop.
- [x] 3.2 Implementar chunks, budgets de instrução/ciclo/evento/wall-time, watchdog e cancelamento cooperativo.
- [x] 3.3 Implementar snapshot consistente de CPU/threads/scheduler/trace e stop reasons estruturados.
- [x] 3.4 Testar lifecycle, races, cancelamento, loop infinito e duas execuções determinísticas.

## 4. ABI e desktop
- [x] 4.1 Evoluir ABI C para 1.5 com handles e structs sized de control/snapshot/diagnostic preservando 1.0–1.4.
- [x] 4.2 Implementar Rust async/RAII/cancelamento sem lógica de emulação e testar teardown.
- [x] 4.3 Implementar controles experimentais React, budgets, status, registradores, primeiro bloqueio e trace acessíveis.
- [x] 4.4 Garantir UI responsiva, sem polling irrestrito, com confirmação/avisos honestos e mídia local.

## 5. Qualidade
- [x] 5.1 Executar CTest normal e ASan+UBSan, format, clangd, C11 legacy, thread/race review e ownership review.
- [x] 5.2 Executar Rust fmt/clippy/test/build e npm lint/typecheck/test/build/Tauri CLI.
- [x] 5.3 Atualizar CI, README, ARCHITECTURE, docs e diário com métricas e limitações.
- [x] 5.4 Validar parent e child changes strict, completar manifesto, integrar em main e remover worktrees somente após branches integradas.