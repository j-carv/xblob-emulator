# Emulação de Kernel e Serviços de Sistema (libs/kernel)

*(Subsistema planejado para marcos futuros)*

## Responsabilidade
Implementar a emulação de alto nível (HLE) da tabela de exportações do kernel do Xbox (thunks de kernel), gerenciamento de threads e processos do guest, subsistema de arquivos FATX e serviços de rede/título.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/memory`, `libs/cpu`.
