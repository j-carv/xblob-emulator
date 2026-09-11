## Purpose

Fornece serviços de kernel reimplementados clean-room para guests sintéticos, sem carregar, copiar ou depender do kernel proprietário.

## ADDED Requirements

### Requirement: Registry versionado de exports
O kernel HLE SHALL registrar exports suportados por ordinal e nome diagnóstico, rejeitando duplicatas e retornando unsupported para desconhecidos.

#### Scenario: Ordinal desconhecido
- **WHEN** guest chama ordinal não implementado
- **THEN** sessão pausa/falha de modo estruturado sem saltar para memória arbitrária

### Requirement: ABI guest explícita
Cada handler SHALL validar argumentos na pilha/memória guest, comprimentos, alinhamento e ponteiros antes de efeitos; retornos e status usam tipos de 32 bits documentados.

#### Scenario: Ponteiro guest inválido
- **WHEN** DbgPrint recebe faixa não mapeada ou string sem terminador dentro do limite
- **THEN** retorna erro seguro sem leitura fora da memória guest

### Requirement: Serviços fundamentais
O HLE SHALL fornecer versões mínimas determinísticas de debug output, alocação/liberação de heap guest, criação/encerramento de thread cooperativa e event/mutex wait/signal necessárias às fixtures.

#### Scenario: Heap allocate/free
- **WHEN** guest aloca bloco alinhado e o libera uma vez
- **THEN** range não sobrepõe alocações vivas e double-free é rejeitado

#### Scenario: Wait e signal
- **WHEN** thread espera evento não sinalizado e outra o sinaliza
- **THEN** scheduler bloqueia e torna a thread runnable em ordem determinística

### Requirement: Proveniência clean-room
Código e documentação SHALL identificar fontes públicas/conceituais utilizadas e MUST NOT incluir headers, símbolos copiados, binários, chaves ou dados do SDK proprietário.

#### Scenario: Auditoria de conteúdo
- **WHEN** revisão pesquisa fixtures e documentação
- **THEN** apenas dados sintéticos e referências públicas permissivas estão presentes
