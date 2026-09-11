## Purpose

Executa programas diagnósticos exclusivamente sintéticos de forma limitada, observável e reproduzível através da sessão e interfaces públicas.

## ADDED Requirements

### Requirement: Thunks sintéticos
O loader SHALL validar e materializar imports sintéticos explicitamente permitidos, ligando-os ao dispatcher HLE sem endereços host ou código proprietário.

#### Scenario: Import suportado
- **WHEN** fixture importa ordinal HLE registrado
- **THEN** thunk chama exatamente o handler correspondente e retorna ao guest

### Requirement: Threads cooperativas
A máquina SHALL manter contextos guest isolados e escaloná-los cooperativamente com budgets e ordem estável, sem criar thread host por thread guest.

#### Scenario: Duas threads runnable
- **WHEN** ambas estão prontas no mesmo ciclo
- **THEN** a ordem documentada é reproduzida em execuções repetidas

### Requirement: Trace estruturado e limitado
A execução SHALL opcionalmente registrar instruções, faults, HLE calls e mudanças de thread em ring buffer limitado, sem dados host sensíveis.

#### Scenario: Overflow de trace
- **WHEN** eventos excedem capacidade
- **THEN** os eventos retidos e contador de descartes são determinísticos e memória permanece limitada

### Requirement: Controle seguro
ABI e UI SHALL permitir somente execução diagnóstica claramente rotulada, com budgets obrigatórios, cancelamento e estados finais; arquivos comerciais/não suportados MUST permanecer apenas inspecionáveis.

#### Scenario: XBE não elegível
- **WHEN** mídia não satisfaz perfil sintético suportado
- **THEN** ação diagnóstica é indisponível e a UI explica a limitação sem alegar compatibilidade

### Requirement: Reprodutibilidade end-to-end
Fixtures SHALL cobrir chamada HLE, heap, duas threads, sincronização, interrupção e término, comparando estado final e trace em duas execuções.

#### Scenario: Reexecução idêntica
- **WHEN** mesma fixture e budgets executam duas vezes
- **THEN** estado, ciclos, status e trace são byte-a-byte equivalentes
