## Why

Um emulador de Xbox clássico apto a ser publicado exige, desde o início, fronteiras arquiteturais, portabilidade, validação de entradas e engenharia de qualidade compatíveis com um projeto de longa duração. Esta mudança estabelece essa fundação sem confundir o primeiro incremento compilável com a meta final de compatibilidade perfeita.

## What Changes

- Criar um monorepo C/C++20 modular, compilável com CMake em macOS, Linux e Windows.
- Definir regras permanentes do projeto em `AGENTS.md`, incluindo arquitetura, qualidade, legalidade, portabilidade e manutenção obrigatória do diário.
- Publicar `ARCHITECTURE.md` na raiz com visão ideal, limites de subsistemas, fluxo de emulação e roadmap de compatibilidade.
- Introduzir bibliotecas separadas para tipos comuns, I/O seguro, formatos de mídia e parsing de XBE, além de um executável CLI fino.
- Implementar inspeção inicial e segura de arquivos `.xbe`, `.iso` e `.xiso`, com detecção por conteúdo quando aplicável e erros estruturados; execução de jogos fica explicitamente fora deste primeiro incremento.
- Adicionar testes unitários, fixtures sintéticas, análise estática/sanitizers opcionais, formatação e CI multiplataforma.
- Criar `diary/` e registrar decisões, progresso, comandos, testes, limitações e próximos passos em Markdown.
- Documentar política de conteúdo protegido: nenhum BIOS, chave, firmware, SDK proprietário ou jogo será incluído.

## Capabilities

### New Capabilities

- `repository-foundation`: Estrutura, build, testes, CI, documentação e governança do monorepo multiplataforma.
- `media-inspection`: Identificação e inspeção defensiva inicial de executáveis XBE e imagens ISO/XISO sem executar conteúdo convidado.

### Modified Capabilities

_Nenhuma._

## Impact

A mudança cria a estrutura integral inicial do repositório, configura CMake e CI, introduz APIs públicas internas de parsing/I/O, uma CLI de inspeção e documentação técnica. Dependências externas devem ser mínimas, multiplataforma, fixadas ou gerenciadas de forma reproduzível e compatíveis com distribuição pública.