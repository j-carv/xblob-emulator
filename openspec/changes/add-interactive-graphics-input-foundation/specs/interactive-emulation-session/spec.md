## Purpose

Combina execução, frames e input em uma sessão responsiva, bounded e observável.

## ADDED Requirements

### Requirement: Frame loop bounded
A sessão SHALL executar CPU/scheduler/GPU em slices limitados, publicar somente frames completos e respeitar pause/stop/watchdog sem bloquear UI.

#### Scenario: Pause durante frame
- **WHEN** pause é solicitado durante processamento
- **THEN** sessão alcança safe point bounded e mantém último frame completo

### Requirement: Input snapshot
Estado host normalizado SHALL ser amostrado por sequence/timestamp guest e entregue ao dispositivo XID sem race ou dependência direta de UI.

#### Scenario: Input durante pause
- **WHEN** botões mudam com sessão pausada
- **THEN** snapshot mais recente é aplicado deterministicamente ao retomar

### Requirement: Métricas e diagnóstico
A sessão SHALL reportar frame sequence, resolução, ciclos, input sequence, métodos GPU e requests USB não suportados em buffers bounded.

#### Scenario: Método gráfico ausente
- **WHEN** pushbuffer chama método desconhecido
- **THEN** execução pausa ou continua conforme política explícita e relatório identifica class/method/subchannel

### Requirement: Teardown seguro
Stop/close SHALL cancelar eventos, cessar callbacks e liberar buffers/dispositivos sem use-after-free.

#### Scenario: Fechar janela ativa
- **WHEN** UI fecha durante execução
- **THEN** worker termina dentro do timeout e nenhum callback toca recursos destruídos
