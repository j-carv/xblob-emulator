# Diário de Engenharia: Bootstrap e Fundação da Produção (2026-09-10)

## 1. Contexto e Objetivos

Início do desenvolvimento do projeto **xblob**, um emulador para o console Xbox original (2001) concebido como um produto de software público, modular e sustentável para os próximos anos.
Neste primeiro marco (Marco 1: Bootstrap & Fundação de Mídia), o objetivo central é assentar as fundações de engenharia com qualidade de produção antes de implementar qualquer subsistema de emulação dinâmica:
- Criar a estrutura normativa de governança (`AGENTS.md`), arquitetura (`ARCHITECTURE.md`), documentação pública (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `LICENSE`, `docs/CONTENT_POLICY.md`);
- Organizar a árvore modular monorepo (`apps/`, `libs/`, `tests/`, `cmake/`, `docs/`, `tools/`);
- Estabelecer uma infraestrutura de compilação moderna C/C++20 com CMake, presets, alertas rigorosos e sanitizers (ASan/UBSan);
- Desenvolver os blocos fundamentais de I/O defensivo (`libs/common`, `libs/io`), garantindo parsing estritamente seguro por cursores de bytes sem *type punning* ou *reinterpret_cast* de structs;
- Implementar parsing e inspeção de executáveis XBE e imagens de disco ISO/XISO sintéticos (`libs/formats`);
- Disponibilizar a primeira ferramenta pública: `xblob-inspect` (`apps/inspect`), acompanhada de testes unitários CTest e workflow de CI para macOS, Linux e Windows.

---

## 2. Decisões Técnicas Tomadas

1. **Monorepo com Bibliotecas Estáticas e Dependências Unidirecionais**:
   - Para evitar arquiteturas monolíticas ou o surgimento de "god files", o repositório é particionado em bibliotecas com responsabilidades focadas.
   - `libs/common` provê os tipos `Result<T, E>`, códigos de erro e aritmética segura (`CheckedAdd`, `SafeRange`).
   - `libs/io` abstrai leitura de bytes e manipulação de arquivo com cursor (`BinaryReader`).
   - `libs/formats` implementa parsers de XBE e detecção de ISO sem depender de subsistemas de apresentação ou emulação.
   - `apps/inspect` é uma camada fina de linha de comando.
2. **Parsing Defensivo sem Cast de Buffers**:
   - `reinterpret_cast<const Struct*>(ptr)` é estritamente proibido. Todas as leituras ocorrem campo a campo com validação prévia de faixas e conversão little-endian explícita, prevenindo comportamento indefinido (UB) decorrente de desalinhamento ou incompatibilidade de layout de tipos entre compiladores.
3. **Legalidade e Fixtures 100% Sintéticas**:
   - Nenhuma ROM de BIOS, chaves criptográficas de console ou jogos proprietários serão permitidos no repositório.
   - Testes geram fixtures sintéticas válidas e malformadas em tempo de teste.
4. **Toolchains e CI**:
   - Suporte primário a AppleClang (macOS), GCC/Clang (Linux) e MSVC (Windows).

---

## 3. Comandos Executados e Resultados de Verificação

### 3.1 Ambiente Host
- macOS ARM64
- Compilador: AppleClang 21.0.0
- CMake: 4.4.3
- Ninja: 1.13.2
- Clang-Format: 23.1.1

### 3.2 Compilação Padrão e Testes CTest
```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```
Resultado do CTest:
```text
Test project /Users/fl4k/Documents/projects/xblob/build/default
    Start 1: test_common
1/4 Test #1: test_common ......................   Passed    0.36 sec
    Start 2: test_io
2/4 Test #2: test_io ..........................   Passed    0.36 sec
    Start 3: test_formats
3/4 Test #3: test_formats .....................   Passed    0.42 sec
    Start 4: test_inspector_integration
4/4 Test #4: test_inspector_integration .......   Passed    0.37 sec

100% tests passed out of 4
Total Test time (real) = 1.53 sec
```

### 3.3 Verificação com AddressSanitizer e UndefinedBehaviorSanitizer
```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan
```
Resultado do CTest sob sanitizers:
```text
Test project /Users/fl4k/Documents/projects/xblob/build/asan-ubsan
    Start 1: test_common
1/4 Test #1: test_common ......................   Passed    0.88 sec
    Start 2: test_io
2/4 Test #2: test_io ..........................   Passed    0.47 sec
    Start 3: test_formats
3/4 Test #3: test_formats .....................   Passed    0.48 sec
    Start 4: test_inspector_integration
4/4 Test #4: test_inspector_integration .......   Passed    0.48 sec

100% tests passed out of 4
Total Test time (real) = 2.32 sec
```
Nenhum vazamento de memória, estouro de buffer, acesso desalinhado ou comportamento indefinido foi detectado.

### 3.4 Verificação de Estilo e Formatação
```bash
./tools/check-format.sh
```
Resultado:
```text
==> Verificando formato de código C/C++...
==> Todos os arquivos C/C++ verificados estão em conformidade com o padrão.
```

### 3.5 Testes da CLI `xblob-inspect`
- `xblob-inspect --help`: Retorna código 0 e exibe sinopse completa.
- `xblob-inspect`: Retorna código 1 e exibe mensagem de uso.
- `xblob-inspect /caminho/inexistente.xbe`: Retorna código 2 (`FileNotFound`).
- `xblob-inspect /tmp/corrompido.xbe`: Retorna código 3 (`InvalidMagic` / `UnknownFormat`).
- `xblob-inspect /tmp/padrao.iso` (ISO 9660 sem XDVDFS): Retorna código 4 (`UnsupportedFormat`).
- `xblob-inspect /tmp/valido.xbe`: Retorna código 0 e exibe relatório estruturado completo do cabeçalho, certificado e seções.
- `xblob-inspect /tmp/valido.iso` (XISO Trimmed): Retorna código 0 e exibe relatório estruturado do descritor XDVDFS.

---

## 4. Limitações Conhecidas

- **Escopo do Marco 1**: O emulador neste momento não decodifica instruções x86, não emula registradores de GPU NV2A e não carrega o kernel Xbox. Apenas analisa e valida a integridade de contêineres e formatos de mídia de forma estática e defensiva.
- **Validação de Linux e Windows via CI**: A compilação e os testes com GCC, Clang Linux e MSVC Windows foram estruturados no workflow `.github/workflows/ci.yml` do GitHub Actions, sendo executados automaticamente nos runners de CI.

---

## 5. Próximos Passos (Marco 2)

1. **libs/memory**: Implementação do espaço de endereçamento plano de 64MB/128MB com proteção de páginas e mapeamento MMU do Xbox.
2. **libs/core**: Scheduler determinístico para sincronização de ciclos e eventos temporizados entre subsistemas.
3. **libs/cpu**: Decodificador inicial de instruções x86-32 e modelo básico de registradores inteiros.

---

## 6. Incremento: Configuração de Tooling do clangd (`configure-clangd-compdb`)

### 6.1 Contexto e Decisões
- **Diagnósticos Espúrios**: O `clangd` não localizava automaticamente o compilation database gerado em `build/default`, resultando em 16 erros falsos ao analisar arquivos como `libs/io/src/file_source.cpp` por falta de resolução dos include directories de `xblob_common` e `xblob_io`.
- **Configuração Portável**: Adicionado arquivo `.clangd` na raiz com `CompileFlags.CompilationDatabase: build/default`, mantendo suporte agnóstico a editor e compatível com macOS, Linux e Windows sem requerer symlinks ou acoplamento a extensões de IDE.
- **Documentação**: Atualizado `CONTRIBUTING.md` com instruções detalhadas sobre a geração do banco pelo preset padrão (`cmake --preset default`) e o procedimento para reiniciar o language server.

### 6.2 Comandos Executados e Resultados de Verificação
1. **Verificação do clangd**:
   ```bash
   clangd --check=libs/io/src/file_source.cpp
   ```
   Resultado:
   - Configuração `.clangd` e banco `build/default/compile_commands.json` carregados com sucesso.
   - Include directories de `xblob_common` e `xblob_io` resolvidos.
   - `All checks completed, 0 errors` (eliminando os 16 diagnósticos falsos).

2. **Verificação de Formatação**:
   ```bash
   ./tools/check-format.sh
   ```
   Resultado: Todos os arquivos C/C++ em conformidade com o padrão.

3. **Build e Testes CTest**:
   ```bash
   cmake --build --preset default
   ctest --preset default
   ```
   Resultado: 100% dos testes aprovados (4/4 testes passando: `test_common`, `test_io`, `test_formats`, `test_inspector_integration`).
