# Gerenciamento de Memória Guest e MMIO (libs/memory)

Este subsistema gerencia o armazenamento de memória RAM física do guest, o espaço de endereçamento de 32 bits, permissões de acesso e o roteamento de MMIO (*Memory-Mapped I/O*).

## Estado Atual (Marco 3: Ativo)
- Alocação e gerenciamento de RAM física configurável para 64 MiB (varejo) ou 128 MiB (devkit), com inicialização em zero e checagem defensiva de limites.
- Mapeamento de regiões no espaço de 32 bits com detecção atômica de sobreposições (rejeição total na colisão sem deixar estado inconsistente).
- Suporte a operações de leitura e escrita de 8, 16 e 32 bits little-endian, permitindo acessos desalinhados contidos na mesma região e rejeitando acessos que cruzem fronteiras de região antes de qualquer mutação.
- Aplicação estrita de permissões de leitura, escrita e execução (`MemoryPermission`), com suporte a fetch de instruções via `Fetch8` e `FetchBytes`.
- Roteamento tipado de MMIO com suporte a larguras de 8, 16 e 32 bits, retornando falha em larguras não suportadas sem fallback silencioso para RAM.
- **Paginação Virtual IA-32 de 4 KiB (`VirtualMemory`)**:
  - Tradução em dois níveis (PDE/PTE) de endereços virtuais para físicos.
  - Registros de controle arquiteturais CR0 (PG - Paging, WP - Write Protect) e CR3 (Page Directory Base Register).
  - Verificação de privilégios (CPL 0 Supervisor vs CPL 3 User) e permissões de página (Presente, Escrita, Usuário).
  - TLB com suporte a invalidação explícita (`InvalidateTlb`, `InvalidatePage`).
  - Geração precisa de códigos de falha de página (#PF) com bits P, W/R, U/S, RSVD, I/D.
  - Escrita virtual transacional (`WriteVirtualBytes`) com reversão total em caso de falha.
- **Adaptador de Barramento (`BusMemoryAdapter`)**: Mapeamento de janelas do barramento guest para o espaço de endereçamento.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/bus`.
- Proibido depender de `libs/cpu`, `libs/core`, `libs/loader` ou `libs/machine`.
