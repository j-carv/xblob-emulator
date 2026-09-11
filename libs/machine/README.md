# Sessão Determinística da Máquina Convidada (libs/machine)

Este subsistema gerencia a máquina de estados e o ciclo de vida completo da sessão de execução convidada, integrando memória, barramento, paginação, CPU e escalonador.

## Estado Atual (Marco 3: Ativo)
- `MachineState`: Estados explícitos de ciclo de vida (`Created`, `Prepared`, `Paused`, `Faulted`, `Stopped`).
- `MachineSession`: Orquestrador determinístico que mantém a composição dos componentes do sistema convidado (`Bus`, `VirtualMemory`, `DeterministicScheduler`, `Cpu`).
- Métodos de controle: `PrepareXbe` (orquestra inspeção, planejamento e carregamento transacional), `Step` (executa ciclo individual), `RunWithBudget` (executa lote limitado de ciclos virtuais), `Pause`, `Resume`, `Reset`.
- Diagnóstico estático: Consulta do estado da sessão, ponto de entrada mapeado, tamanho de imagem e metadados de preparação.

## Limites Arquiteturais e Legais
- O ciclo de vida neste marco suporta preparação de imagens e execução de instruções sintéticas de teste.
- Não há suporte a carregamento de jogos comerciais nem execução do kernel Xbox oficial.
- Nenhum código proprietário ou firmware é necessário.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/bus`, `libs/core`, `libs/memory`, `libs/cpu`, `libs/loader`.
- Proibido depender de `apps/*` ou `libs/c_api`.
