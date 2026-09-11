## Purpose

Garante que a stack desktop C++/Rust/TypeScript seja reproduzível, auditável e validada continuamente em todos os sistemas operacionais suportados.

## ADDED Requirements

### Requirement: Dependências reproduzíveis
Dependências Node e Rust SHALL possuir lockfiles versionados, versões mínimas documentadas e licenças compatíveis com distribuição pública.

#### Scenario: Instalação limpa
- **WHEN** um colaborador usa as versões documentadas em clone limpo com acesso aos registros oficiais
- **THEN** instalações frozen/locked resolvem exatamente as dependências versionadas

### Requirement: Verificações frontend automatizadas
O frontend SHALL possuir comandos não interativos para lint, typecheck, testes e build de produção.

#### Scenario: Alteração de componente
- **WHEN** a suíte frontend é executada
- **THEN** lint, tipos, testes e build concluem sem exigir shell Tauri aberto

### Requirement: Verificações do adaptador
O adaptador Rust SHALL passar por formatação, lint estrito e testes, mantendo comandos IPC separados da lógica de domínio.

#### Scenario: Validação Cargo
- **WHEN** o adaptador é validado em CI
- **THEN** `cargo fmt`, `cargo clippy` com warnings negados e testes concluem com sucesso

### Requirement: Matriz desktop multiplataforma
A CI SHALL validar a combinação CMake, frontend e Tauri em macOS, Linux e Windows, instalando somente dependências de sistema documentadas.

#### Scenario: Pull request desktop
- **WHEN** arquivos da ABI, frontend ou shell mudam
- **THEN** a matriz executa verificações relevantes nos três hosts e falha diante de incompatibilidade

### Requirement: Build sem warnings estruturais
Targets CMake e executáveis de teste SHALL evitar bibliotecas transitivas duplicadas e warnings de link conhecidos.

#### Scenario: Rebuild limpo no macOS
- **WHEN** os testes são relinkados no host AppleClang
- **THEN** o linker não reporta bibliotecas xblob duplicadas

### Requirement: Operação offline após instalação
Testes automatizados MUST NOT acessar rede nem depender de mídia proprietária após as dependências terem sido instaladas.

#### Scenario: Suíte local
- **WHEN** testes C++, Rust e React executam com caches/dependências presentes e rede indisponível
- **THEN** usam apenas fixtures sintéticas e concluem sem chamadas externas
