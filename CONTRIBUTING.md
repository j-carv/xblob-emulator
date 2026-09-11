# Guia de Contribuição (CONTRIBUTING.md)

Agradecemos o interesse em contribuir com o **xblob**! Este projeto segue padrões rígidos de engenharia de software, modularidade, qualidade e segurança jurídica.

Antes de enviar contribuições, leia atentamente as diretrizes abaixo e as regras normativas descritas em [AGENTS.md](AGENTS.md).

---

## 1. Princípios de Contribuição

1. **Desenvolvimento Clean-Room**:
   - Todo código adicionado deve ser produto de pesquisa em documentação aberta, engenharia reversa limpa ou implementações originais.
   - É terminantemente vedado o uso de código descompilado do Xbox SDK proprietário, código-fonte vazado de sistemas operacionais ou patentes não licenciadas.
2. **Proibição de Binários Proprietários**:
   - Não envie PRs contendo imagens de BIOS, ROMs, chaves de autenticação ou dumps de mídias protegidas por direitos autorais.
   - Qualquer necessidade de teste deve ser satisfeita através de fixtures sintéticas geradas programaticamente.
3. **Padrão C++20 e Modularidade**:
   - O código deve compilar sem avisos (*warnings*) nas toolchains Clang, GCC e MSVC.
   - Evite arquivos acumuladores de muitas funções (*god files*). Mantenha unidades focadas, com responsabilidade única e dependências estritamente unidirecionais.
4. **Parsing Seguro**:
   - Sempre utilize as abstrações de `libs/io` (`BinaryReader`, checagem de faixas) para ler dados binários. Não faça cast de buffers para structs (`reinterpret_cast`).

---

## 2. Fluxo de Trabalho e Pull Requests

1. **Criar uma Branch**:
   - Crie uma branch com nome descritivo para sua alteração: `git checkout -b feature/sua-melhoria`.
2. **Configuração de Ambiente e Banco de Compilação (clangd)**:
   - Para habilitar suporte de linguagem preciso no editor (clangd) com autocompletar e diagnósticos sem falsos positivos:
     ```bash
     cmake --preset default
     ```
   - O projeto possui o arquivo `.clangd` na raiz configurado com `CompileFlags.CompilationDatabase: build/default`, apontando para `build/default/compile_commands.json`.
   - Após executar a configuração pela primeira vez ou atualizar alvos/arquivos no CMake, reinicie o language server no editor (por exemplo, no VS Code: abra o Command Palette com `Ctrl+Shift+P` / `Cmd+Shift+P` e selecione `clangd: Restart language server`, ou recarregue a janela).
3. **Compilação**:
   - Compile utilizando o preset padrão:
     ```bash
     cmake --build --preset default
     ```
4. **Formatação de Código**:
   - Execute o script de verificação ou use clang-format diretamente antes de submeter alterações:
     ```bash
     ./tools/check-format.sh # ou clang-format -i <arquivos>
     ```
5. **Testes Automatizados**:
   - Verifique que todos os testes passam localmente utilizando o preset padrão:
     ```bash
     ctest --preset default
     ```
6. **Atualização do Diário**:
   - Conforme especificado em [AGENTS.md](AGENTS.md), contribuições significativas devem registrar contexto, testes e decisões em `diary/`.

---

## 3. Código de Conduta

- Mantenha a comunicação respeitosa, objetiva e orientada a evidências técnicas.
- Dúvidas técnicas podem ser discutidas nos canais de issues do repositório.
