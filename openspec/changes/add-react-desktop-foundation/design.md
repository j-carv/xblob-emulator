## Context

O repositório possui núcleo C++ modular, `MediaInspector`, CLI funcional e testes CTest. Não existe ABI C, workspace Node, código Rust ou GUI. O host atual já dispõe de Node/npm e Rust/Cargo, mas a solução deve ser reproduzível em macOS, Linux e Windows. O frontend não pode assumir que o núcleo executa jogos; a única vertical apropriada é inspeção de mídia.

## Goals / Non-Goals

**Goals:**

- Entregar aplicativo Tauri abrível e uma build web testável sem shell nativo.
- Criar fronteira ABI reutilizável por futuras UIs e ferramentas.
- Demonstrar ponta a ponta: diálogo nativo → IPC → Rust → ABI C → C++ → DTO → React.
- Estabelecer arquitetura frontend por features, sistema visual e acessibilidade desde o início.
- Integrar toolchains e CI sem degradar a suíte C++ existente.

**Non-Goals:**

- Executar XBE, criar sessão de emulação, manter biblioteca de jogos ou implementar GPU/áudio/kernel.
- Colocar regras de parsing, emulação ou classificação de erros no Rust/TypeScript.
- Estabilizar para sempre cada campo da ABI major 1 antes de uso público; evolução compatível será governada por versão/tamanho.
- Assinar/notarizar instaladores nesta change.
- Baixar ou incluir mídia Xbox.

## Decisions

### Estrutura do monorepo desktop

Criar `apps/desktop/` com `src/` para React/TypeScript, `src-tauri/` para shell Rust e arquivos de tooling locais. Organizar React em `app/`, `features/inspection/`, `features/about/`, `components/`, `bridge/`, `styles/` e `test/`. O arquivo de aplicação compõe rotas/providers; cada feature controla seus componentes e estado. Alternativa rejeitada: um `App.tsx` com todos os fluxos.

### React + Vite + TypeScript estrito

Usar React com Vite, TypeScript em modo strict, ESLint e Vitest/Testing Library. React Router fornece rotas explícitas. Evitar framework web full-stack e state manager global enquanto não houver estado que justifique isso; estado assíncrono da inspeção fica em hook/reducer da feature. Alternativa rejeitada: Next.js, pois SSR/servidor não agrega valor ao desktop local.

### Design system leve e próprio

CSS custom properties definem tokens semânticos de cor, spacing, tipografia, radius, shadow, motion e z-index. Componentes primitivos focados (`Button`, `Card`, `Status`, `AppShell`) usam HTML semântico. Usar fonte de sistema para evitar downloads em runtime. Tema `system/light/dark` é aplicado antes da renderização e persistido localmente. Ícones devem ter licença permissiva e labels acessíveis.

### ABI C em target dedicado

Criar `libs/c_api` com header C compilável como C11 e implementação C++ que traduz `MediaInspector`. Exportar macro de visibilidade encapsulado apenas nessa fronteira. A ABI major 1 usa:

- consulta de versão/capabilities;
- status enum numérico estável;
- handle opaco para resultado de inspeção;
- getters sem alocação cruzada ou cópia por buffer two-call;
- função destroy que aceita null e não lança;
- função para recuperar último erro por objeto/resultado, não por global mutável.

Toda função `extern "C"` captura exceções e converte em status interno. Caminhos entram como UTF-8 + tamanho; entrada é validada antes de formar `filesystem::path`. Alternativas rejeitadas: expor structs C++/JSON alocado pela biblioteca ou usar bindings diretos para classes C++.

### Build Rust↔C++ reproduzível

O `build.rs` usa o crate `cmake` para configurar uma build isolada do núcleo com apps/testes desligados e instalar os targets C API necessários. O CMake ganha regras de install para bibliotecas/headers públicos. O script emite links estáticos e runtime C++ específicos por toolchain em um ponto isolado; não espalhar condicionais de plataforma no domínio. Bindings FFI são declarados manualmente a partir do header pequeno, evitando dependência de libclang/bindgen. O Cargo permanece locked.

Se a integração do crate `cmake` com multi-config MSVC exigir ajuste, ele deve estar contido em `build.rs` e validado na CI Windows; não aceitar path absoluto ou biblioteca pré-compilada no repositório.

### Adaptador Tauri mínimo

Rust define DTOs serializáveis e dois comandos: `get_core_info` e `inspect_media`. O file picker é fornecido pelo plugin oficial de diálogo Tauri com allowlist mínima. `inspect_media` executa trabalho bloqueante fora da thread principal, chama a ABI, copia dados para DTO owned e destrói o handle em RAII. Nenhum parser ou regra de emulação é reimplementado.

### Bridge frontend injetável

TypeScript depende de uma interface `DesktopBridge`; implementação Tauri usa `invoke`/dialog, enquanto testes usam fake determinístico. Componentes nunca importam APIs Tauri diretamente. Isso permite `npm run dev` no navegador com bridge de desenvolvimento controlada e testes offline.

### Segurança da shell

Content Security Policy restritiva, sem CDN, sem acesso remoto, sem shell/process plugin e sem leitura arbitrária pelo frontend. O diálogo retorna somente o caminho escolhido ao comando; bytes são lidos pelo núcleo. Capabilities Tauri concedem apenas diálogo e comandos necessários. Logs não expõem conteúdo ou dados sensíveis do path além do necessário na UI local.

### UX inicial

Tela Home comunica “fundação técnica” e oferece inspeção. Rota Inspect contém drop-style call-to-action que abre diálogo (não implementar drag-and-drop irrestrito nesta fase), histórico apenas da sessão, estado loading e painéis de metadados. About mostra versões da UI/core/ABI e capacidades reais. Não há botão Play. Layout possui sidebar em janela ampla e navegação compacta em largura reduzida.

### Qualidade e CI

Adicionar scripts `npm ci`, `lint`, `typecheck`, `test --run`, `build`, e comandos Cargo `fmt --check`, `clippy -- -D warnings`, `test`. CI deve cachear apenas artefatos seguros e instalar dependências Tauri documentadas em Linux. Validar web em Linux e Rust/Tauri nos três hosts quando tecnicamente suportado. CTest passa a linkar cada teste apenas às dependências necessárias, eliminando warnings duplicados.

Testes cobrem:

- ABI em C++ incluindo header por unidade `.c` de compile-smoke;
- Rust mapping/RAII com seam de FFI quando possível;
- React bridge, estados, rotas, tema e axe;
- build integrada sem bundle/assinatura.

### Dependências e licenças

Fixar lockfiles npm/Cargo e registrar dependências diretas, propósito e licença em documentação. A CLI Tauri v2 deve ser uma devDependency npm local, invocada pelo script `npm run tauri`, para que desenvolvimento e CI não dependam de instalação global. Builds podem acessar registros oficiais para instalar dependências; testes após instalação são offline. Não versionar `node_modules`, `dist`, `target`, `*.tsbuildinfo`, cobertura, caches ou `src-tauri/gen`; manter versionados fontes, lockfiles, configurações, capabilities e ícones necessários. A verificação deve usar `git status --porcelain -uall` para distinguir arquivos realmente untracked dos milhares já corretamente ignorados.

## Risks / Trade-offs

- [Tauri/Rust aumenta a matriz de toolchains] → Versões mínimas, lockfiles, CI e scripts únicos documentados.
- [Link estático C++ via Cargo varia por toolchain] → Centralizar no `build.rs`, usar CMake como fonte de verdade e testar nos três hosts.
- [ABI inicial pode crescer rapidamente] → Version major/minor, structs sized quando necessárias, handles e testes de compatibilidade.
- [WebView varia entre hosts] → CSS baseado em padrões amplamente suportados e smoke build por plataforma.
- [A UI pode parecer mais capaz que o núcleo] → Capability query, textos honestos e ausência explícita de controles de execução.
- [Dependências frontend ampliam supply chain] → Poucas dependências diretas, lockfile, audit documentado sem autoalterações e CSP offline.
- [Build Tauri completo pode depender de libs Linux ausentes localmente] → Documentar pacotes, validar na CI e não alegar plataforma não executada.
- [Warning duplicado do linker indica target tests excessivamente acoplado] → Links específicos por teste e rebuild limpo como gate desta change.

## Migration Plan

1. Corrigir links dos testes CMake e confirmar rebuild sem warning conhecido.
2. Criar/testar `libs/c_api` de forma independente, incluindo compile-smoke C11.
3. Criar workspace React e arquitetura de bridge com implementação fake/testes.
4. Construir páginas, design tokens, temas e acessibilidade.
5. Criar shell Tauri, build.rs/FFI, comandos e file picker restrito.
6. Integrar a vertical real de inspeção e testes por camada.
7. Adicionar CI/documentação/diário e executar todas as verificações locais disponíveis.

A CLI e APIs C++ existentes permanecem. A ABI é nova e começa na major 1; rollback remove `libs/c_api` e `apps/desktop` sem afetar os targets anteriores.