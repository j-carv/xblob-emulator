# Revisão do marco gráfico e entrada interativa

**Data**: 14 de setembro de 2026
**Base revisada**: `edbcebb..integration/interactive-graphics-input`

## Contexto e decisões

A revisão independente dos commits `3834453`, `2825d74` e `73cfc9d` encontrou dois problemas reais: estado de transferências de controle OHCI em mapa global compartilhado entre controladores (com risco de corrida e vazamento entre sessões), e acessos XID fora do mutex. Também havia uma corrida de lifecycle no loop React: uma operação assíncrona podia reagendar `requestAnimationFrame` após desmontagem/pausa.

## Progresso realizado

- Tornado o estado OHCI de controle propriedade da instância do traversal/controller, com limpeza no reset.
- Protegidos endereço, sequência e estado de rumble XID com o mutex existente.
- Adicionado gate de ciclo de vida no painel React e limpeza de teclas pressionadas no teardown.

## Comandos e resultados

- `cmake --build build -j2`: sucesso.
- `ctest --test-dir build --output-on-failure`: 23/23 sucesso.
- `npm run typecheck`: sucesso.
- `npm test -- --run`: 38/38 sucesso.
- `./tools/check-format.sh`: sucesso.
- `git diff --check`: sucesso.

## Limitações e próximos passos

O marco continua uma fundação experimental bounded; não demonstra jogabilidade de títulos comerciais. Permanecem riscos de cobertura incompleta de OHCI/NV2A e de validação de ABI em consumidores externos além dos smoke tests locais.
