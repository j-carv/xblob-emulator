# Loader Transacional de Executáveis XBE (libs/loader)

Este subsistema implementa o carregamento defensivo e transacional em duas fases de imagens executáveis XBE sintéticas na memória virtual paginada.

## Estado Atual (Marco 3: Ativo)
- **Carregamento Transacional em Duas Fases**:
  - **Fase 1 (`Plan`)**: Análise puramente em memória sem efeitos colaterais. Valida integridade do cabeçalho, tabela de seções, limites de endereçamento virtual, alinhamentos de 4 KiB e conformidade W^X (não simultaneamente W e X).
  - **Fase 2 (`Apply`)**: Aplicação atômica na memória virtual (`VirtualMemory`). Todas as páginas das seções são mapeadas e populadas com seus respectivos dados. Se qualquer falha ocorrer durante o processo (como esgotamento de memória física ou falha de escrita), um rollback atômico reverte todas as páginas alocadas nesta operação.
- **Configuração de Contexto IA-32 (`CreateInitialContext`)**: Inicialização determinística do contexto arquitetural da CPU a partir do ponto de entrada do XBE, alocação de pilha sintética protegida e configuração de seletores mínimos.

## Limites Arquiteturais e Legais
- Este módulo não realiza decodificação de criptografia proprietária ou descriptografia com chaves privadas.
- Opera exclusivamente sobre executáveis sem criptografia de cabeçalho ou com chaves nulas documentadas publicamente para testes e homebrew de domínio público.
- Nenhuma BIOS ou ROM oficial é utilizada ou requerida.

## Fronteiras e Dependências
- **Dependências permitidas**: `libs/common`, `libs/formats`, `libs/memory`.
- Proibido depender de `libs/cpu` (exceto tipos arquiteturais através de contexto compartilhado se aplicável), `libs/bus` ou `libs/machine`.
