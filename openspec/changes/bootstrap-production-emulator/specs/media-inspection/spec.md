## Purpose

Define a primeira superfície observável para reconhecer e inspecionar com segurança contêineres e executáveis usados pelo Xbox clássico, sem executar código convidado.

## ADDED Requirements

### Requirement: Inspeção de XBE
A ferramenta SHALL aceitar um caminho para arquivo XBE, validar assinatura e limites estruturais antes de acessar campos e apresentar metadados básicos disponíveis sem executar o binário.

#### Scenario: XBE estruturalmente válido
- **WHEN** o usuário solicita inspeção de um XBE sintético com cabeçalho e limites válidos
- **THEN** a ferramenta identifica o formato e apresenta metadados básicos de forma determinística

#### Scenario: XBE truncado ou malformado
- **WHEN** o usuário fornece um arquivo cuja assinatura sugere XBE mas cujos offsets ou tamanhos excedem o arquivo
- **THEN** a ferramenta rejeita a entrada com erro claro e termina sem leitura fora de limites

### Requirement: Inspeção de ISO e XISO
A ferramenta SHALL reconhecer imagens `.iso` e `.xiso` suportadas por evidência estrutural documentada, diferenciando formato reconhecido, formato ainda não suportado e entrada inválida.

#### Scenario: Imagem reconhecida
- **WHEN** o usuário fornece uma fixture sintética que contém a estrutura mínima válida de uma variante suportada
- **THEN** a ferramenta identifica a variante e apresenta os metadados disponíveis

#### Scenario: Extensão enganosa
- **WHEN** um arquivo arbitrário recebe extensão `.iso` ou `.xiso`
- **THEN** a ferramenta não o declara válido apenas pela extensão

### Requirement: Erros estruturados e saída estável
A inspeção SHALL retornar categorias de erro distinguíveis para I/O, formato desconhecido, conteúdo truncado, campo inválido e recurso ainda não suportado, com códigos de saída documentados.

#### Scenario: Arquivo inexistente
- **WHEN** o usuário solicita inspeção de um caminho inexistente
- **THEN** a ferramenta informa erro de I/O e retorna código diferente de sucesso

### Requirement: Processamento defensivo de entrada não confiável
Todo parser SHALL tratar conteúdo de jogos e imagens como não confiável, validar aritmética de offsets e tamanhos e ter testes para limites, truncamentos e entradas aleatórias relevantes.

#### Scenario: Offset causa overflow
- **WHEN** um campo de entrada produziria overflow ao calcular uma faixa
- **THEN** o parser rejeita o campo antes de realizar acesso à memória ou ao arquivo
