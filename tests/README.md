# Suíte de Testes e Validação (tests/)

Este diretório contém os testes automatizados do projeto **xblob**, organizados segundo o princípio de testes determinísticos e reproduzíveis.

## Estrutura

- `unit/`: Testes unitários para bibliotecas individuais (`libs/common`, `libs/io`, `libs/formats`).
- `integration/`: Testes de integração de ponta a ponta para a CLI `xblob-inspect` e a fachada de inspeção.
- `fixtures/`: Geradores de dados sintéticos e fixtures em memória.

## Regra de Legalidade para Fixtures
**PROIBIÇÃO TOTAL**: Nenhuma ROM, BIOS de hardware, chave criptográfica real, imagem de jogo comercial ou binário proprietário pode ser colocado nesta pasta.
Todas as entradas de teste devem ser:
1. Geradas programmaticamente em tempo de compilação/teste; ou
2. Arquivos sintéticos gerados por ferramentas abertas com metadados puramente fictícios.
