## Why

O build CMake está correto, mas o clangd do editor não descobre automaticamente o banco localizado em `build/default`, produzindo diagnósticos falsos de includes e símbolos ausentes. A configuração do workspace deve oferecer análise estática coerente sem depender de caminhos absolutos da máquina local.

## What Changes

- Adicionar configuração portátil do clangd apontando para o diretório do banco de compilação do preset padrão.
- Documentar como gerar o banco e reiniciar/recarregar o language server.
- Verificar a configuração com o clangd disponível e repetir build/testes para garantir que tooling não afete o produto.
- Registrar a correção no diário da sessão.

## Capabilities

### New Capabilities

_Nenhuma; trata-se exclusivamente de tooling do editor._

### Modified Capabilities

_Nenhuma._

## Impact

Afeta somente configuração de desenvolvimento e documentação. Não altera APIs, parsers, formato de saída, comportamento em runtime ou suporte de plataforma.