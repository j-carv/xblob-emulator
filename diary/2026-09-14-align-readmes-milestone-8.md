# Alinhamento dos READMEs com o Marco 8

**Data:** 14 de setembro de 2026

**Change:** `align-readmes-with-milestone-8`

## Contexto e decisões

O usuário solicitou corrigir diretamente referências obsoletas nos READMEs e autorizou explicitamente o bypass do fluxo de delegação. A documentação raiz já anunciava o Marco 8 no título, mas o aviso principal ainda declarava Marco 7; o README do desktop ainda descrevia ABI C 1.3 e negava qualquer execução de mídia comercial.

As descrições foram alinhadas ao estado integrado: Marco 8, ABI C 1.6 (retrocompatível com contratos 1.0–1.5), execução interativa apenas experimental bounded e conteúdo estritamente local, sem alegações de compatibilidade ou jogabilidade comercial. Foi mantida a distinção rigorosa entre tentativa experimental e compatibilidade, renderização correta ou jogabilidade demonstrada.

## Progresso realizado

- Corrigido o aviso de estado no `README.md` para Marco 8.
- Atualizada a descrição da sessão interativa, NV2A e entrada OHCI/XID.
- Confirmadas as referências de ABI C para 1.6 retrocompatível com contratos 1.0–1.5 nos dois READMEs.
- Removida do README desktop a afirmação obsoleta de que não existe execução, substituindo-a pelo escopo experimental verificado com mídia local.
- Preservados os avisos de conteúdo local, fixtures sintéticas e ausência de suporte ou compatibilidade comercial declarada.

## Comandos e verificações

- Busca textual por `Marco 7`, ABI 1.3, alegações de compatibilidade e afirmações de jogabilidade: nenhuma referência obsoleta ou alegação indevida permaneceu nos READMEs.
- `git diff --check`: sucesso.
- Revisão do diff restrita a documentação e artefatos OpenSpec.
- `openspec validate align-readmes-with-milestone-8 --strict`: sucesso.

## Limitações e próximos passos

A atualização documental não amplia a compatibilidade do emulador. Boot completo, gráficos corretos e gameplay de títulos comerciais continuam não demonstrados. A change permanece ativa e não deve ser arquivada sem autorização explícita.
