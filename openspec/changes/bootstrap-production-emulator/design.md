## Context

O workspace contém apenas a infraestrutura OpenSpec recém-inicializada. O produto final pretende reproduzir um sistema x86 de 32 bits com GPU NV2A, MCPX/APU, memória, kernel e dispositivos próprios, enquanto o host inicial é macOS e os demais hosts são Linux e Windows. Conteúdo convidado é não confiável e alguns materiais necessários ao uso podem ter distribuição legalmente restrita. Consulte `proposal.md` e as specs desta change para o contrato do primeiro incremento.

## Goals / Non-Goals

**Goals:**

- Criar uma base C/C++20 pequena, compilável e estritamente modular.
- Fazer a portabilidade depender de interfaces estreitas, não de condicionais espalhadas.
- Disponibilizar uma vertical inicial real: leitura segura e inspeção de XBE/ISO/XISO sintéticos.
- Tornar build, testes e convenções executáveis por CI desde o primeiro commit.
- Documentar a arquitetura-alvo completa sem fingir que ela já está implementada.

**Non-Goals:**

- Executar instruções convidadas, HLE/LLE de kernel, renderizar NV2A, emular áudio ou inicializar jogos nesta change.
- Incluir material proprietário ou contornar mecanismos de proteção.
- Fixar prematuramente backend gráfico, estratégia JIT ou modelo definitivo de sincronização antes de microbenchmarks e testes de conformidade.
- Alegar compatibilidade com jogos a partir da simples inspeção de seus contêineres.

## Decisions

### Monorepo orientado a bibliotecas e dependências direcionais

Usar CMake com targets pequenos e uma árvore previsível: `apps/`, `libs/`, `tests/`, `cmake/`, `docs/`, `tools/` e `third_party/` quando necessário. Bibliotecas de nível mais baixo não conhecem frontend; formatos dependem apenas de utilidades e I/O; o futuro núcleo dependerá de contratos de plataforma. Isso permite testes isolados e evita god files. Alternativa rejeitada: um executável único dividido apenas por headers, pois permite acoplamento implícito e ciclos.

### C++20 no domínio, C somente em fronteiras justificadas

C++20 fornece RAII, `std::span`, tipos de soma e ownership explícito para parsing seguro; C fica disponível para APIs estáveis, componentes de baixo nível ou integração externa quando trouxer vantagem concreta. Exceções não atravessam fronteiras públicas internas: resultados esperados de parsing usam tipo explícito de sucesso/erro. Alternativa rejeitada: C puro em todo o sistema, devido ao custo de ownership e composição segura para a escala pretendida.

### Portabilidade por contratos

Relógio, memória virtual, threads, arquivos, janelas, áudio e gráficos serão encapsulados em interfaces de plataforma. Código específico de macOS/Linux/Windows fica em unidades próprias. Endianness, largura e layout nunca dependem implicitamente do ABI host. Alternativa rejeitada: macros condicionais em código de domínio.

### Arquitetura-alvo em camadas

O plano raiz deverá definir: frontend/headless; sessão e scheduler determinístico; loader e formatos; CPU x86 (intérprete de referência antes de backend dinâmico); memória/MMU; kernel e serviços; barramentos e dispositivos; NV2A; APU/áudio; input; persistência; telemetria e depuração. O primeiro incremento implementa apenas common/I/O/formats/CLI, mas reserva nomes e direção de dependência coerentes com essa decomposição.

### Parsing limitado e orientado a cursor

Leitura binária utiliza bytes, offsets verificados e operações que comprovam `offset + size` sem overflow antes de formar views. O parser não faz casts de buffers para structs de host. Detecção considera assinaturas/estrutura, usando extensão somente como pista. A implementação inicial pode reportar `unsupported` para variantes cuja validação robusta ainda não foi construída.

### Dependências mínimas e explícitas

Preferir biblioteca padrão e código pequeno neste bootstrap. Framework de testes só será incorporado se puder ser adquirido de forma reproduzível ou vendorizado com licença clara; caso contrário, um runner interno pequeno e específico é aceitito temporariamente. Toda dependência futura exige licença, versão, justificativa e matriz de suporte.

### Qualidade e compatibilidade mensuráveis

Warnings elevados, testes unitários, fixtures sintéticas, CI em três hosts e sanitizers onde suportados compõem a porta inicial. A evolução do emulador deve adicionar testes diferenciais de CPU, golden tests de GPU/áudio, fuzzing de parsers, determinismo e uma matriz pública de compatibilidade. “Perfeito” é meta baseada em evidências por título e subsistema, nunca rótulo sem teste.

### Documentação operacional

`AGENTS.md` na raiz será normativo para humanos e agentes; `ARCHITECTURE.md` descreverá estado atual versus arquitetura-alvo; `diary/YYYY-MM-DD-bootstrap.md` registrará esta execução. Alterações futuras deverão atualizar documentação e diário quando decisões ou estado mudarem.

## Risks / Trade-offs

- [Escopo do emulador é de vários anos e alta complexidade] → Entregar por marcos verticais, manter specs por subsistema e medir compatibilidade continuamente.
- [Documentar muitos módulos antes de implementá-los pode criar arquitetura especulativa] → Marcar claramente alvo versus implementado e revisar decisões a cada marco.
- [C++20 não elimina comportamento indefinido] → Ownership explícito, sanitizers, fuzzing, parsing sem casts e revisão de aritmética.
- [Diferenças entre toolchains quebram portabilidade] → CI real em AppleClang, Clang/GCC e MSVC; nenhum host vira implementação de referência exclusiva.
- [ISO de Xbox possui layouts e offsets distintos de ISO9660 convencional] → Detecção baseada em estrutura, fixtures por variante e erros `unsupported` em vez de heurísticas permissivas.
- [Pesquisa de emulação pode tocar material legalmente sensível] → Clean-room, fontes públicas registradas, nenhum blob proprietário e responsabilidade do usuário por conteúdo legalmente obtido.
- [Abstrações prematuras podem prejudicar desempenho] → Contratos estreitos, medição antes de otimização e possibilidade de backends especializados sem contaminar domínio.

## Migration Plan

1. Criar documentação e governança.
2. Adicionar esqueleto de diretórios e configuração CMake com targets independentes.
3. Implementar resultado/erro, leitor binário e fontes de arquivo.
4. Implementar detecção e inspeção mínima defensiva de XBE e imagens reconhecíveis.
5. Expor apenas uma CLI fina e estável para a vertical.
6. Adicionar testes, fixtures geradas, ferramentas e CI multiplataforma.
7. Executar validações locais disponíveis e registrar resultados no diário.

Como o repositório não possui consumidores anteriores, não há migração de API. Em caso de falha, cada target pode ser removido isoladamente; documentação e OpenSpec permanecem como fonte para correção.