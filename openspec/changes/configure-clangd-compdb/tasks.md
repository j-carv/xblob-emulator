## 1. Configuração do clangd

- [x] 1.1 Adicionar `.clangd` na raiz com caminho relativo para `build/default`; verificar a configuração carregando `libs/io/src/file_source.cpp` com `clangd --check` e o banco existente.
- [x] 1.2 Confirmar que o banco de compilação contém os include directories públicos de `xblob_common` e `xblob_io`; verificar que `file_source.cpp` não apresenta erros de include ou símbolos desconhecidos.

## 2. Documentação e regressão

- [x] 2.1 Documentar no guia de contribuição como gerar o banco pelo preset padrão e reiniciar o language server; verificar que os comandos e caminhos correspondem ao `CMakePresets.json` atual.
- [x] 2.2 Executar formatação, build e CTest para confirmar ausência de regressões e registrar comandos/resultados no diário datado.
