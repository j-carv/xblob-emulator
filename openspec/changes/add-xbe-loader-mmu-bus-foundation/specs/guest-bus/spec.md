## Purpose

Define um barramento convidado modular para conectar regiões MMIO e dispositivos sem acoplar memória, CPU ou sessão a implementações específicas.

## ADDED Requirements

### Requirement: Registro não sobreposto
O barramento SHALL registrar dispositivos em faixas físicas explícitas e MUST rejeitar ranges vazios, com overflow ou sobreposição sem alterar registros existentes.

#### Scenario: Dispositivos sobrepostos
- **WHEN** um segundo dispositivo intersecta faixa registrada
- **THEN** o registro falha atomicamente

### Requirement: Despacho MMIO
Leituras e escritas SHALL ser encaminhadas uma única vez ao dispositivo correspondente com offset relativo, largura e valor corretos.

#### Scenario: Acesso válido
- **WHEN** uma escrita de 32 bits atinge registrador suportado
- **THEN** somente o dispositivo proprietário recebe o acesso

#### Scenario: Endereço sem dispositivo
- **WHEN** acesso MMIO não corresponde a faixa registrada
- **THEN** retorna fault unmapped sem fallback silencioso

### Requirement: Dispositivo de diagnóstico sintético
O projeto SHALL fornecer dispositivo sintético de registradores para testes, sem modelar falsamente hardware Xbox real.

#### Scenario: Round trip de registrador
- **WHEN** teste escreve e lê registrador sintético
- **THEN** valor e log de acessos são determinísticos
