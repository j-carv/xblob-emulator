## Purpose

Transforma paradas de títulos em dados acionáveis e reproduzíveis.

## ADDED Requirements

### Requirement: Primeiro bloqueio estruturado
O sistema SHALL distinguir opcode/form unsupported, kernel export, GPU method, file service, exception, watchdog e internal error.

#### Scenario: Export ausente
- **WHEN** dispatcher encontra ordinal desconhecido
- **THEN** relatório inclui ordinal, EIP, thread, call count e categoria sem crash host

### Requirement: Snapshot limitado
Diagnóstico SHALL incluir registradores, flags, threads, scheduler, últimos eventos e stack words validados em buffers bounded.

#### Scenario: Stack inválida
- **WHEN** ESP aponta para memória não mapeada
- **THEN** snapshot marca stack unavailable sem falhar novamente

### Requirement: Reprodutibilidade
A mesma fixture e configuração SHALL produzir mesmo stop reason, contadores e trace normalizado.

#### Scenario: Duas execuções
- **WHEN** fixture roda duas vezes
- **THEN** relatórios são equivalentes exceto campos host-time explicitamente excluídos
