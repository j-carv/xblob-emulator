# Aplicações e Interfaces (apps/)

Este diretório contém os pontos de entrada executáveis (frontends e utilitários de linha de comando) do projeto **xblob**.

## Princípios de Projeto

- As aplicações em `apps/` devem ser camadas finas de apresentação e despacho.
- **Nenhuma lógica de parsing, emulação ou I/O de baixo nível** deve residir diretamente nos executáveis. Todo o trabalho deve ser delegado às bibliotecas em `libs/`.
- Dependências permitidas: `apps/*` podem depender de `libs/*`. Nenhuma biblioteca em `libs/*` pode depender de `apps/*`.

## Alvos

- `inspect/`: Utilitário de linha de comando `xblob-inspect` para análise e validação estática de executáveis e mídias.
- *(Futuro)* `gui/`: Interface gráfica multiplataforma com renderização de vídeo e menus.
- *(Futuro)* `headless/`: Modo headless para execução de testes em lote e depuração remota.
