# Diário de Bordo: Integração Gráfica e de Entrada Interativa

**Data**: 14 de setembro de 2026
**Change**: `add-interactive-graphics-input-foundation`

## Contexto e decisões

A integração foi concluída na branch `integration/interactive-graphics-input`, preservando o trabalho não commitado já existente e os commits das lanes GPU e USB/XID já integrados. A sessão conecta NV2A 3D, OHCI/XID, IRQ, publicação bounded de frames, snapshots sincronizados e adaptadores C/Rust/Tauri/React. O diagnóstico é orientado por capacidade e não consulta nome, ID, hash ou heurística de título. Nenhum conteúdo proprietário foi adicionado; fixtures permanecem sintéticas.

A verificação de elegibilidade por nome de caminho foi removida do adaptador Tauri: caminhos não são evidência confiável de proveniência. A política de validação sintética é documental e de testes, não uma heurística de produto.

## Progresso

- Mantida a integração de MachineSession, ABI C 1.6 e painel interativo pré-existentes.
- Atualizados README, ARCHITECTURE e proveniência clean-room para refletir o Marco 8 e suas limitações honestas.
- Todos os 17 itens do plano parent foram marcados após verificação; integração em `main`, push e remoção de worktrees não fazem parte deste pedido.

## Comandos e resultados

- `cmake -S . -B build ...` e `cmake --build build -j2`: sucesso.
- `ctest --test-dir build --output-on-failure`: 23/23 sucesso.
- Build/teste com ASan/UBSan em `build-sanitize`: 23/23 sucesso.
- `./tools/check-format.sh`: sucesso.
- `clangd --check` foi iniciado, mas excedeu o limite do ambiente durante a análise; o compile database e a compilação C++ permanecem válidos.
- `cargo fmt --check`, `cargo clippy --locked -- -D warnings`, `cargo test --locked`: sucesso.
- `npm run lint`, `typecheck`, `test` (38 testes), `build`: sucesso.
- `openspec validate ... --strict` para parent, `expand-nv2a-3d-pipeline` e `add-usb-xid-input`: sucesso.

## Limitações e próximos passos

A execução interativa de jogos comerciais continua meta futura e não é declarada como compatibilidade. A validação local futura, incluindo qualquer título escolhido pelo usuário, não deve produzir fixtures, heurísticas ou dados proprietários no repositório. O gate Tauri CLI completo depende das bibliotecas gráficas do host quando executado fora do CI; cargo build/test validaram o adaptador Rust.
