# Diário de Bordo: Fundação Desktop React + Tauri v2 e ABI C

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação integral da change OpenSpec `add-react-desktop-foundation`.

---

## 1. Eliminação do Warning de Bibliotecas Duplicadas no Linker

Ao vincular a suíte de testes com AppleClang, o linker emitia warnings recorrentes de bibliotecas duplicadas (`ld: warning: ignoring duplicate libraries: 'libxblob_common.a'`).

**Causa Raiz**:
Em `tests/CMakeLists.txt`, o alvo agregador `xblob_test_helpers` exportava `xblob_common` e `xblob_io` via `INTERFACE`, enquanto os testes individuais (`test_safe_math`, `test_xbe_parser`, etc.) linkavam concorrentemente `xblob_test_helpers` e as bibliotecas específicas do subsistema. Em combinatórias com dependências transitivas, os alvos eram especificados múltiplas vezes no comando final do compilador/linker.

**Solução Aplicada**:
Refatoramos `tests/CMakeLists.txt` para um modelo estrito de dependências por teste:
- Removemos a linkagem recursiva do agregador.
- Cada executável de teste declara estritamente suas dependências diretas (`xblob_common`, `xblob_io`, `xblob_formats`, `xblob_memory`, `xblob_core`, `xblob_cpu`, `xblob_c_api`).
- Resultado: **Zero warnings de duplicação** na compilação padrão e na suíte ASan+UBSan, com todos os testes passando (10/10 no CTest).

---

## 2. Encapsulamento da ABI C (`libs/c_api`)

Para desacoplar qualquer interface de usuário do código C++ interno e viabilizar múltiplos frontends, implementamos a biblioteca `libs/c_api`:

1. **Pureza C11**:
   - Header público `xblob/c_api.h` estritamente compatível com C11 e compilável por qualquer compilador C puro sem extensões.
   - Nomes prefixados uniformemente com `xblob_`.
2. **Versionamento Semântico e Segurança**:
   - Versionamento de ABI `1.0.0` com campos explícitos de `struct_size` para permitir evolução binária sem quebrar chamadores legados.
   - Constantes de bitmask de capacidades negociáveis (`xblob_get_capabilities`).
3. **Isolamento de Memória e Ciclo de Vida**:
   - Handles opacos (`xblob_media_report_t`) alocados na heap do C++ e destruídos exclusivamente via `xblob_media_report_destroy`.
   - Pattern de buffer em duas chamadas (`two-call pattern`) para cópia segura de strings UTF-8, eliminando transferências implícitas de ponteiro.
   - Barreira estrita `noexcept` com bloco `try/catch (...)` em todas as funções C para prevenir vazamento de exceções C++ através da fronteira FFI.

---

## 3. Arquitetura do Frontend React & Bridge Desacoplado

O frontend (`apps/desktop`) foi desenvolvido em React 19 + TypeScript com Vite, estruturado sob uma interface de contrato (`BridgeContract`):

- **TypeScript Estrito**:
  - `no-explicit-any` configurado como erro no ESLint.
  - Tipos DTOs (`CoreInfo`, `MediaReport`, `XbeMetadata`, `XisoMetadata`, `AppError`) sincronizados 1:1 com os dados providos pela ABI C.
- **Transparência de Execução**:
  - `TauriBridge`: invoca comandos IPC nativos via `@tauri-apps/api/core`.
  - `MockBridge`: provê fixtures sintéticas completas de XBE e XISO para desenvolvimento e testes em navegador puro sem necessidade do binário nativo compilado.
  - Seleção automática baseada em detecção de ambiente Tauri ou variável `VITE_USE_MOCK`.

---

## 4. Shell Tauri v2 e Adaptador FFI em Rust

A pasta `apps/desktop/src-tauri` atua exclusivamente como ponte fina e segura:

- **Isolamento de Domínio**:
  - O Rust **não contém nenhuma lógica de parsing ou inspeção**. Todo o domínio reside no C++ e é acessado via `extern "C"`.
- **Adaptador RAII Seguro**:
  - O struct Rust `SafeMediaReport` gerencia o handle opaco C e implementa a trait `Drop`, assegurando desalocação determinística sem risco de vazamento de memória.
- **Postura de Segurança Restrita**:
  - Content Security Policy (CSP) restritivo: sem `unsafe-eval`, conexão com rede desabilitada (`connect-src 'none'`).
  - Capabilities v2 limitadas estritamente a IPC interno e plugin nativo de diálogo de arquivos. Acesso a shell e escrita arbitrária em disco são estritamente desabilitados.

---

## 5. Validação de Acessibilidade (WCAG 2.2 AA)

Garantimos acessibilidade e usabilidade profissional:
- **Design Tokens**: Variáveis CSS definindo paleta com contraste comprovado superior a `4.5:1` para texto padrão e `3:1` para elementos gráficos, em modos escuro e claro.
- **Navegação por Teclado**:
  - `SkipLink` para acesso imediato ao conteúdo principal.
  - Foco visível com `outline-offset` de 2px em todos os elementos interativos.
  - Componente de abas (`Tabs`) com navegação acessível por setas (`ArrowLeft`, `ArrowRight`, `Home`, `End`), `aria-selected`, e papéis semânticos WAI-ARIA.
  - Modal com captura de foco (focus trap) e fechamento por `Escape`.
- **Suporte a Movimento Reduzido**: Respeito a `@media (prefers-reduced-motion: reduce)` em todas as animações e transições.
- **Testes Automatizados**: Integração de `vitest-axe` com `axe-core`, garantindo zero violações de acessibilidade em todas as telas e componentes nos testes unitários.

---

## 6. Escopo Legal e Técnico Inegociável

Reafirmamos no código, na interface (`AboutModal`) e na documentação que o xblob é uma ferramenta de pesquisa histórica e inspeção defensiva de formatos. Nenhuma funcionalidade de execução de jogos comerciais ou ações "Play/Run" está presente ou planejada.

---

## 7. Higiene de Repositório: Distinção entre Arquivos Untracked e Ignorados

Em conformidade com a tarefa 6.9 da change `add-react-desktop-foundation`, configuramos `.gitignore` e `apps/desktop/.gitignore` para garantir o isolamento estrito de artefatos efêmeros e caches gerados pelo ecossistema TypeScript e Tauri:

- **Regras adicionadas**:
  - `*.tsbuildinfo` (caches incrementais do compilador TypeScript).
  - `coverage/` e `.eslintcache` (cobertura de testes e caches de linter).
  - `apps/desktop/src-tauri/gen/` e `src-tauri/gen/` (esquemas e manifestos ACL autogerados pelo Tauri).
- **Preservação comprovada**:
  - Fontes C++, Rust e TypeScript (`src/`, `src-tauri/src/`, `libs/`).
  - Lockfiles determinísticos (`package-lock.json`, `Cargo.lock`).
  - Arquivos de configuração (`tauri.conf.json`, `tsconfig*.json`, `vite.config.ts`).
  - Capabilities oficiais da aplicação (`apps/desktop/src-tauri/capabilities/default.json`).
  - Ativos visuais e ícones (`apps/desktop/src-tauri/icons/`).
- **Validação de Ignorados e Contagem Real**:
  - Total de arquivos untracked legítimos (`git status --porcelain=v1 -uall`): **203 arquivos** (fontes, testes, specs, documentação e diários da change).
  - Total de arquivos ignorados (`git status --ignored --porcelain=v1 -uall`): **18.321 arquivos** ignorados pelo Git (incluindo `node_modules/`, diretórios de build `build*`, caches e targets Rust).
  - Confirmação via `git check-ignore -v` de que os 6 arquivos gerados anteriormente expostos (`tsconfig.app.tsbuildinfo`, `tsconfig.node.tsbuildinfo`, e os 4 esquemas em `src-tauri/gen/schemas/`) estão devidamente ignorados e nenhum diretório como `dist/`, `target/` ou `node_modules/` permanece exposto como untracked.
