## Why

O núcleo já possui inspeção segura de mídia e uma primeira máquina sintética, mas o produto público precisa de uma superfície gráfica acessível e escalável desde cedo para evitar que UX e integração sejam acopladas tardiamente. Esta mudança cria uma vertical desktop real — seleção e inspeção de mídia — sem apresentar controles de execução que o emulador ainda não suporta.

## What Changes

- Criar uma ABI C versionada em target próprio, sem tipos C++ na fronteira, expondo versão/capacidades e inspeção de mídia por handles opacos com ownership explícito.
- Criar `apps/desktop` com React + TypeScript, Vite e Tauri v2, mantendo Rust restrito a comandos IPC e FFI.
- Entregar shell visual moderno com navegação, página inicial, fluxo de inspeção, estados vazio/loading/sucesso/erro e página de informações/capacidades.
- Integrar file picker nativo e inspeção real pelo núcleo C++, sem ler arquivos no frontend e sem executar código convidado.
- Adicionar design tokens, temas claro/escuro/sistema, layout responsivo, acessibilidade por teclado, foco e contraste.
- Adicionar testes C++ da ABI, testes Rust do adaptador e testes unitários/de componentes React com bridge mockável.
- Integrar lint, typecheck, build web, Cargo e verificações desktop à CI de macOS, Linux e Windows, com lockfiles e licenças revisáveis.
- Corrigir a composição dos targets CTest para eliminar bibliotecas transitivas duplicadas e warnings do linker detectados na revisão do Marco 2.
- Atualizar documentação e diário com requisitos, comandos, estado real e instruções de desenvolvimento desktop.
- Garantir que dependências, builds, caches e schemas gerados pela stack desktop sejam ignorados pelo Git sem ocultar fontes, lockfiles ou configurações necessárias.

## Capabilities

### New Capabilities

- `core-c-api`: Contrato binário C estável e seguro para consumidores externos consultarem versão, capacidades e inspeção de mídia.
- `desktop-media-inspection`: Aplicativo desktop React/Tauri para selecionar mídia e apresentar resultados de inspeção do núcleo.
- `desktop-experience`: Sistema visual, navegação, responsividade, acessibilidade, temas e estados de interface consistentes.
- `desktop-build-quality`: Build, testes, dependências e CI reproduzíveis para a stack C++/Rust/TypeScript nos três hosts.

### Modified Capabilities

_Nenhuma._

## Impact

A mudança adiciona `libs/c_api`, `apps/desktop`, toolchains Node/npm e Rust/Cargo, dependências Tauri/React e novos jobs de CI. O núcleo C++ continua a fonte única da lógica de domínio. A CLI existente permanece compatível. Nenhuma funcionalidade de carregar/executar jogos é introduzida.