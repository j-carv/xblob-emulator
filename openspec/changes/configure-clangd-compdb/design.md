## Context

O CMake já exporta `build/default/compile_commands.json`, e os targets publicam corretamente seus include directories. O clangd aberto na raiz não pesquisa subdiretórios de build arbitrários, portanto analisa os `.cpp` sem os argumentos reais do target.

## Goals / Non-Goals

**Goals:**

- Fazer o clangd localizar o banco do preset `default` por configuração relativa à raiz.
- Manter a solução válida em macOS, Linux e Windows.
- Documentar geração e recarga do índice.

**Non-Goals:**

- Alterar includes, APIs ou CMake targets para satisfazer diagnósticos sem contexto.
- Versionar banco de compilação gerado.
- Impor uma extensão específica de VS Code.

## Decisions

Adicionar `.clangd` na raiz com `CompileFlags.CompilationDatabase: build/default`. Essa é uma configuração nativa, pequena e independente do editor. A alternativa de criar symlink `compile_commands.json` na raiz foi rejeitada porque symlinks têm experiência inconsistente no Windows; configuração exclusiva em `.vscode/settings.json` foi rejeitada por acoplar o projeto a um editor.

Documentar que `cmake --preset default` deve ser executado antes da indexação e que o clangd precisa ser reiniciado após criar ou atualizar o banco.

## Risks / Trade-offs

- [O banco ainda não existe em clone novo] → Documentar o comando de configuração antes da primeira indexação.
- [Usuário trabalha exclusivamente em outro preset] → Manter `default` como banco canônico de desenvolvimento; presets alternativos continuam destinados a build/teste.
- [clangd antigo não reconhece a chave] → Verificar sintaxe com a versão instalada e documentar versão mínima quando necessário.

## Migration Plan

Adicionar a configuração e documentação sem migração de código. Para rollback, remover `.clangd`; o build permanece inalterado.