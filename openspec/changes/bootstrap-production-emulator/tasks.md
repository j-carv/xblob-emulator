## 1. Governança e arquitetura

- [x] 1.1 Criar `AGENTS.md` normativo na raiz com as regras do produto, modularidade, portabilidade, segurança, legalidade, qualidade e atualização obrigatória do diário; verificar que cobre explicitamente macOS, Linux e Windows e proíbe god files e artefatos proprietários.
- [x] 1.2 Criar `ARCHITECTURE.md` na raiz distinguindo estado implementado e arquitetura-alvo, com diagrama textual, responsabilidades, dependências, fluxo de execução e roadmap por marcos; verificar que todos os grandes subsistemas do console têm fronteira descrita.
- [x] 1.3 Adicionar documentação pública inicial (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, licença e política de conteúdo) e verificar consistência entre suporte declarado, estado real e ausência de alegação prematura de compatibilidade.
- [x] 1.4 Criar `diary/` e o registro datado deste bootstrap com decisões e comandos iniciais; verificar que o arquivo contém progresso, testes, limitações e próximos passos.

## 2. Fundação do monorepo

- [x] 2.1 Criar árvore modular em `apps/`, `libs/`, `tests/`, `cmake/`, `docs/` e `tools/`, com README curto nas áreas futuras quando necessário; verificar que cada target planejado possui responsabilidade única e direção de dependência documentada.
- [x] 2.2 Configurar CMake C/C++20 com presets portáveis, opções de warnings, sanitizers e análise estática sem flags exclusivas aplicadas ao compilador errado; verificar configuração e build fora da árvore no macOS atual.
- [x] 2.3 Adicionar convenções de formatação, `.editorconfig`, `.gitignore` e comandos de tooling; verificar `clang-format` em modo check quando disponível.

## 3. Infraestrutura segura de formatos

- [x] 3.1 Implementar tipos comuns de erro/resultado e aritmética de faixas verificada em biblioteca independente; verificar testes de overflow, limites exatos e propagação de erros.
- [x] 3.2 Implementar fonte de bytes/arquivo somente leitura e leitor binário orientado a cursor sem casts de buffer para structs; verificar testes de leitura little-endian, truncamento, seek e arquivos inexistentes.
- [x] 3.3 Implementar detecção e inspeção mínima de XBE com assinatura, campos básicos e validação de faixas; verificar fixtures sintéticas válidas, truncadas, com assinatura inválida e offsets maliciosos.
- [x] 3.4 Implementar detecção conservadora de ISO/XISO por conteúdo, incluindo a estrutura mínima validada que o projeto suportar neste marco e resultado `unsupported` para variantes não implementadas; verificar que extensões enganosas são rejeitadas.
- [x] 3.5 Criar fachada de inspeção que selecione parser por evidência e retorne metadados/erros estruturados sem executar conteúdo convidado; verificar testes de integração cobrindo XBE, imagem reconhecida, entrada aleatória e I/O.

## 4. Aplicação e interfaces

- [x] 4.1 Criar CLI fina para `inspect <path>` com help, saída humana estável e códigos de saída documentados; verificar casos de sucesso, argumento ausente, arquivo inexistente, formato inválido e formato não suportado.
- [x] 4.2 Garantir que APIs públicas internas não exponham detalhes específicos do host nem ownership ambíguo; verificar por testes de compilação e revisão das dependências de targets.

## 5. Qualidade multiplataforma

- [x] 5.1 Configurar testes via CTest sem depender de material protegido e gerar fixtures sintéticas durante os testes ou build; verificar execução completa local sem rede.
- [x] 5.2 Adicionar workflow de CI para build e testes em macOS, Linux e Windows, com toolchains e comandos equivalentes; verificar sintaxe e matriz dos workflows.
- [x] 5.3 Adicionar alvos opcionais de sanitizers e análise estática onde suportados, sem impedir toolchains incompatíveis; verificar ao menos AddressSanitizer/UndefinedBehaviorSanitizer no host disponível ou documentar objetivamente a indisponibilidade.
- [x] 5.4 Executar configuração, build, testes e inspeções de smoke locais; corrigir falhas e registrar comandos e resultados finais no diário.
- [x] 5.5 Revisar tamanhos e responsabilidades dos arquivos, includes, ciclos, warnings, documentação e diff final; verificar que nenhuma unidade se tornou god file e que o estado declarado corresponde ao entregue.
