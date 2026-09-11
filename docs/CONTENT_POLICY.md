# Política de Conteúdo Protegido e Legalidade (CONTENT_POLICY.md)

Esta política descreve as regras rígidas do projeto **xblob** quanto a propriedade intelectual, engenharia reversa limpa e materiais protegidos por direitos autorais.

---

## 1. Proibição Expressa de Artefatos Proprietários

O repositório do **xblob** é estritamente **livre de artefatos proprietários**. É terminantemente proibido incluir, vincular, distribuir ou solicitar:
1. **Imagens de BIOS e Boot ROMs**:
   - Nenhuma imagem de BIOS original de varejo (*retail*) ou desenvolvimento (*devkit*), nem a ROM oculta de inicialização MCPX (512 bytes) serão fornecidas com o emulador.
2. **Chaves Criptográficas Privadas**:
   - Chaves de assinatura de títulos, chaves de autenticação de disco ou certificados digitais do console não são distribuídos no código-fonte nem em releases binários.
3. **Jogos Comerciais e Dados de Mídia**:
   - Imagens de disco (.iso, .xiso) ou executáveis (.xbe) de jogos comerciais não devem ser armazenados no repositório.
4. **Xbox SDK Proprietário**:
   - Nenhuma biblioteca estática (`.lib`), cabeçalho (`.h`), ferramenta ou binário derivado do Microsoft Xbox SDK oficial pode ser incorporado ao código-fonte.

---

## 2. Testes e Fixtures Sintéticas

Para garantir que a integração contínua (CI) e os testes locais funcionem de forma determinística, independente e 100% legal:
- Todos os testes de leitura de contêineres e formatos utilizam **fixtures sintéticas** criadas programmaticamente em tempo de compilação/teste.
- Os cabeçalhos e estruturas sintéticas utilizam apenas números mágicos documentados em literatura pública e tamanhos mínimos suficientes para exercitar a lógica de parsing sem copiar código proprietário.

---

## 3. Responsabilidade do Usuário

O usuário final do emulador é o único responsável por prover legalmente quaisquer arquivos de mídia, cópias de segurança de seus próprios discos ou dados de configuração de seu próprio console conforme a legislação aplicável em sua jurisdição.
