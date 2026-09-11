# Política de Segurança (SECURITY.md)

A segurança do usuário e a robustez contra arquivos maliciosos são requisitos de primeira ordem no projeto **xblob**.

---

## 1. Escopo e Postura de Segurança

Emuladores de consoles frequentemente processam arquivos de jogos, imagens de disco e binários executáveis provindos de fontes terceiras e não confiáveis. Por isso:
- Consideramos qualquer arquivo `.xbe`, `.iso`, `.xiso` ou imagem de sistema de arquivos como **entrada potencialmente hostil**.
- Tratamos vulnerabilidades de corrupção de memória (como estouro de buffer, divisão por zero, uso após liberação ou leitura fora de limites) durante o processamento de formatos como falhas de segurança críticas.
- O projeto adota ativamente sanitizers (`AddressSanitizer`, `UndefinedBehaviorSanitizer`) e análise estática durante o desenvolvimento.

---

## 2. Como Relatar uma Vulnerabilidade

Se você descobrir uma falha de segurança no `xblob`, pedimos que relate de maneira responsável:

1. **Não abra uma issue pública** contendo os detalhes da vulnerabilidade ou prova de conceito explorável.
2. Envie um e-mail com a descrição detalhada, ambiente e passos para reprodução para:
   - `security@xblob.org` (ou utilize o canal de *Private Vulnerability Reporting* do GitHub, se disponível).
3. Inclua, se possível:
   - Descrição da falha e potencial impacto;
   - Uma fixture sintética mínima que provoque o comportamento anômalo;
   - Informações do sistema operacional e versão do compilador.

Responderemos em até 72 horas úteis confirmando o recebimento do relatório e estimando o prazo de correção e publicação do patch.
