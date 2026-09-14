## Context

A documentação raiz já lista recursos do Marco 8, mas o aviso principal ainda afirma Marco 7. O README do desktop também mantém ABI 1.3 e uma descrição anterior à execução interativa experimental. Como se trata de documentação pública, a correção deve usar somente capacidades verificadas no código e nos gates do marco.

## Goals / Non-Goals

**Goals:**

- Tornar consistentes marco, ABI e descrição do fluxo interativo nos dois READMEs.
- Confirmar Marco 8, ABI C 1.6 retrocompatível com contratos 1.0–1.5, execução interativa apenas experimental e conteúdo estritamente local.
- Diferenciar claramente "tentar execução experimental" de "compatibilidade ou jogabilidade comercial demonstrada".
- Preservar os avisos legais e de privacidade.

**Non-Goals:**

- Alterar código, contratos, UI ou testes.
- Declarar suporte ou compatibilidade a qualquer título comercial específico.
- Arquivar changes OpenSpec existentes.

## Decisions

- Usar o Marco 8 e ABI C 1.6 (retrocompatível com contratos 1.0–1.5) como referências canônicas, pois correspondem ao estado integrado em `main`, sem afirmar compatibilidade ou jogabilidade de jogos comerciais.
- Descrever o desktop como capaz de execução interativa experimental de conteúdo local, mantendo a ressalva de que boot completo, gráficos corretos e gameplay comercial não foram demonstrados.
- Atualizar apenas afirmações obsoletas ou contraditórias; listas técnicas válidas permanecem intactas.
- Registrar a sessão em novo arquivo de `diary/`, conforme governança do repositório.

## Risks / Trade-offs

- [Risco de superestimar compatibilidade] → Usar linguagem explícita de tentativa experimental e listar limitações.
- [Risco de nova divergência documental] → Procurar referências a Marco 7, ABI 1.3 e afirmações absolutas nos READMEs após a edição.
