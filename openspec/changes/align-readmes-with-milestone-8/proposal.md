## Why

Os READMEs contêm referências contraditórias ao Marco 7, ABI C 1.3 e ausência total de execução, enquanto o código integrado está no Marco 8, ABI C 1.6 e oferece execução interativa experimental. A documentação pública deve refletir o estado verificado sem alegar compatibilidade ou jogabilidade comercial.

## What Changes

- Corrigir o marco atual para Marco 8 onde aplicável.
- Atualizar referências da ABI C para 1.6 (retrocompatível com contratos 1.0–1.5), sem fazer alegações de compatibilidade ou jogabilidade comercial.
- Alinhar a descrição do desktop com preparação e execução interativa experimental de mídia local.
- Manter explícito que jogos comerciais, incluindo o título usado pelo usuário em validação local, não possuem compatibilidade ou jogabilidade demonstrada.
- Remover contradições internas sem alterar código, APIs ou comportamento do produto.

## Capabilities

### New Capabilities

Nenhuma. Esta change altera somente documentação e declara `skip_specs: true`.

### Modified Capabilities

Nenhuma. Não há alteração de requisitos ou comportamento observável.

## Impact

Afeta `README.md`, `apps/desktop/README.md` e o diário obrigatório da sessão. Não altera código, dependências, ABI ou testes do produto.
