## 1. Higiene do build existente

- [x] 1.1 Refatorar `tests/CMakeLists.txt` para cada teste linkar apenas targets necessários, preservando fixtures onde usadas; verificar rebuild limpo de todos os testes sem warnings de bibliotecas xblob duplicadas.
- [x] 1.2 Executar CTest padrão e ASan+UBSan após a refatoração de links; verificar 8/8 testes anteriores sem regressão antes de adicionar novos alvos.

## 2. ABI C do núcleo

- [x] 2.1 Criar `libs/c_api` com header público C11, macros de export/calling convention encapsuladas e target CMake; verificar que uma unidade `.c` inclui o header sem construções C++.
- [x] 2.2 Definir versão major/minor, capabilities flags e enum de status com valores estáveis documentados; verificar testes de consulta sem contexto.
- [x] 2.3 Implementar handle opaco de inspeção com criação, getters e destroy null-safe, sem exceções na fronteira; verificar ownership, destruição e chamadas inválidas sob sanitizers.
- [x] 2.4 Implementar inspeção por caminho UTF-8+tamanho delegando ao `MediaInspector`; verificar XBE/XISO sintéticos, formato inválido, arquivo inexistente, UTF-8 inválido e ausência de handle parcial.
- [x] 2.5 Implementar getters de tipo, tamanho, caminho, título/metadados básicos e erro usando buffer two-call ou views com lifetime inequívoco; verificar buffer nulo, pequeno, exato e conteúdo UTF-8.
- [x] 2.6 Adicionar guards de compatibilidade estrutural e captura de falhas internas; verificar tamanho/versão incompatível e conversão determinística para status.
- [x] 2.7 Adicionar install/export CMake dos headers e bibliotecas necessários ao consumidor Rust; verificar configure/build/install isolado com apps e testes desligados.

## 3. Fundação React

- [x] 3.1 Inicializar `apps/desktop` com React 18/19, TypeScript strict e Vite, gerando lockfile npm; verificar `npm run build` e typecheck sem `any`.
- [x] 3.2 Estruturar `apps/desktop/src` em `app/`, `features/`, `components/`, `bridge/`, `styles/` e `test/`; verificar lint e imports sem ciclos.
- [x] 3.3 Definir contratos TypeScript de DTO para `CoreInfo`, `MediaReport`, `XbeMetadata`, `XisoMetadata` e erros; verificar sincronia estrutural com a ABI C.
- [x] 3.4 Implementar camada de bridge com mock para desenvolvimento web e client IPC para Tauri; verificar chaveamento transparente por ambiente/variável de mock.

## 4. Experiência visual e acessibilidade

- [x] 4.1 Definir tokens de design em CSS variables (cores, tipografia, espaçamento, sombras, foco visível); verificar paleta dark/light com contraste >= 4.5:1.
- [x] 4.2 Criar componentes base acessíveis: `Button`, `Card`, `Badge`, `Tabs/Navigation`, `Toast/Alert`, `SkipLink`; verificar roles ARIA e foco navegável por teclado.
- [x] 4.3 Implementar sistema de tema (dark/light/system) persistido sem flash visual; verificar persistência em localStorage e obediência a `prefers-color-scheme`.
- [x] 4.4 Implementar tela inicial com boas-vindas, status do núcleo, capabilities e call-to-action para inspeção; verificar ausência de termos ou ações de emulação jogável.
- [x] 4.5 Implementar fluxo de inspeção com file picker nativo (via bridge), drag and drop opcional e feedback de carregamento; verificar transição fluida de estados.
- [x] 4.6 Implementar painel de visualização com abas/seções para Resumo, Executável XBE, Sistema XDVDFS e Raw/Hex dump sintético; verificar renderização condicional por formato.
- [x] 4.7 Implementar estados de erro amigáveis para formato desconhecido, mídia corrompida e falha de I/O, com ações de recuperação; verificar mensagens acionáveis sem stack trace técnico.
- [x] 4.8 Implementar tela/modal "Sobre" com versão do app, versão do núcleo, licenças e disclaimer explícito de escopo; verificar links e foco preso/restaurado em modais.
- [x] 4.9 Validar acessibilidade automatizada (axe-core ou vitest-axe) e manual por teclado; verificar zero violações graves em telas principais.

## 5. Shell Tauri e FFI

- [x] 5.1 Configurar workspace/módulo Cargo do Tauri v2 com `Cargo.lock`, convenções de target e isolamento da lógica de domínio em C++; verificar que o Rust contém apenas FFI, DTOs e comandos IPC.
- [x] 5.2 Implementar `build.rs` para orquestrar CMake de `libs/c_api`, obter headers/artefatos estáticos e emitir flags de link portáveis; verificar build em árvore limpa com sanitizers desligados por padrão.
- [x] 5.3 Criar módulo FFI com bindings C11 seguros, verificação estrita de versão/tamanho da ABI e sem transferência de propriedade implícita; verificar tratamento de strings não terminadas e paths não-UTF8.
- [x] 5.4 Implementar comandos Tauri `get_core_info`, `inspect_media` e seleção de arquivo delegando ao FFI C e plugins nativos; verificar que strings de erro e DTOs mapeiam 1:1 com o bridge React.
- [x] 5.5 Configurar `tauri.conf.json` e capabilities v2 com CSP restrito, permissões mínimas e bundle info; verificar que requisições arbitrárias, shell e rede estão desabilitados.
- [x] 5.6 Adicionar testes Rust cobrindo conversão FFI -> DTO, caminhos inválidos, erro retornado pelo core e compatibilidade de versão ABI; verificar `cargo test` verde.

## 6. Qualidade, CI e documentação

- [x] 6.1 Adicionar testes C++ da ABI e integrar ao CTest; verificar suíte total padrão e ASan+UBSan com fixtures exclusivamente sintéticas.
- [x] 6.2 Configurar ESLint, TypeScript, Vitest/Testing Library e cobertura focada nos fluxos críticos; verificar lint, typecheck, testes e produção Vite sem warnings.
- [x] 6.3 Configurar `cargo fmt --check`, Clippy estrito e testes Rust locked; verificar todos os comandos no host atual.
- [x] 6.4 Expandir CI para Node/npm, Rust/Cargo, CMake C API e Tauri em macOS, Linux e Windows, incluindo pacotes Linux documentados; verificar sintaxe, lockfiles e ausência de alegação de execução remota local.
- [x] 6.5 Documentar dependências diretas, versões mínimas, licenças, instalação e comandos desktop em README/CONTRIBUTING/docs; verificar compatibilidade de distribuição e ausência de assets/fontes remotos.
- [x] 6.6 Atualizar `ARCHITECTURE.md`, READMEs e estado do produto para marcar a UI inicial como implementada sem afirmar execução de jogos; verificar coerência entre UI, ABI e roadmap.
- [x] 6.7 Criar diário datado com decisões, comandos e resultados reais de C++, Rust e TypeScript, limitações e próximo marco; verificar que falhas/validações não executadas não são omitidas.
- [x] 6.8 Executar format checks C++/Rust/frontend, clangd no C API, builds/testes completos e revisão de segurança/ownership/tamanhos; corrigir problemas e validar `add-react-desktop-foundation --strict` antes de marcar todas as tarefas.
- [x] 6.9 Ignorar artefatos gerados ainda expostos (`*.tsbuildinfo`, caches/cobertura e `apps/desktop/src-tauri/gen/`) sem ignorar fontes, lockfiles, configuração ou ícones; verificar com `git status --porcelain -uall` que node_modules/dist/target e esses gerados não aparecem como untracked e registrar a contagem real no diário.
- [x] 6.10 Adicionar `@tauri-apps/cli` v2 como devDependency bloqueada no lockfile e script npm `tauri`, documentando e verificando `npm run tauri dev`/`npm run tauri -- --version` sem depender de instalação global.
