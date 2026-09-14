## Purpose

Garante que compatibilidade evolua por capacidades gerais e não por hacks específicos de jogos.

## ADDED Requirements

### Requirement: Proibição de hacks por título
Implementação MUST NOT ramificar por título, Title ID, hash, filename ou assinatura de jogo para alterar semântica de CPU/kernel/GPU/input.

#### Scenario: Primeiro título de teste
- **WHEN** usuário testa Star Wars Battlefront II
- **THEN** correções são justificadas por comportamento geral do hardware/API e cobertas por fixtures sintéticas independentes do jogo

### Requirement: Relatório de capacidades
Falhas SHALL ser agregadas por subsystem/capability/identifier/count/first context, permitindo priorização sem armazenar mídia.

#### Scenario: Repetição
- **WHEN** mesmo método ausente ocorre milhares de vezes
- **THEN** relatório bounded agrega contagem sem log ilimitado

### Requirement: Privacidade da mídia
Telemetria SHALL permanecer local e excluir dumps, paths completos e conteúdo proprietário por padrão.

#### Scenario: Exportar diagnóstico
- **WHEN** usuário copia relatório
- **THEN** inclui estado técnico redigido e nenhuma página de código/textura da mídia
