# Diário de Bordo: Fundações de PCI, GPU NV2A, Framebuffer e ABI 1.3

**Data**: 11 de setembro de 2026  
**Contexto**: Implementação da change OpenSpec `add-nv2a-display-foundation`.

---

## 1. Decisão Vinculante e Alinhamento Arquitetural

Durante este marco, consolidamos a diretriz estratégica fundamental do projeto **xblob**:
- **Meta Final do Produto**: O emulador destina-se a carregar e executar/jogar arquivos `.xbe`, `.iso` e `.xiso` fornecidos e possuídos legalmente pelo usuário em seu ambiente local. Conteúdo comercial de propriedade do usuário não é proibido como dado de entrada local da aplicação.
- **Proibição Estrita no Repositório**: É expressamente proibido incorporar, distribuir, comitar ou testar no repositório qualquer jogo comercial, BIOS, chave privada, firmware, kernel proprietário ou SDK oficial.
- **Fixtures Automatizadas**: Todas as baterias de testes automatizados permanecem 100% sintéticas e limpas (*clean-room*).
- **Distinção entre Estado Atual e Meta Final**: O escopo puramente diagnóstico e o preview de tela restrito a mídias sintéticas são limitações temporárias de engenharia deste marco, e não um teto permanente do produto. Botões interativos como "Play" aguardam a validação completa do pipeline funcional de execução.

---

## 2. Subsistema de Barramento PCI (`libs/pci`)

Implementamos uma abstração limpa e determinística do padrão PCI (Peripheral Component Interconnect):
- **Cabeçalho de Configuração Type 0**: Estrutura little-endian de 64 bytes com Vendor ID, Device ID, Command, Status, BARs 0..5, e registradores de interrupção.
- **Endereçamento BDF**: Identificação determinística de dispositivos via tripla (Bus, Device, Function).
- **Dimensionamento e Alocação de BARs**: Protocolo clássico de sizing via escrita de `0xFFFFFFFF` e leitura da máscara de tamanho, verificação de alinhamento natural e detecção rigorosa de colisões de faixas de memória física.
- **Controle de Acesso MMIO**: Transações de leitura e escrita direcionadas aos BARs só são despachadas quando o bit `memory_space` no registrador `command` estiver ativo, respeitando fielmente a especificação PCI Local Bus.

---

## 3. Fundações da GPU NV2A e Framebuffer (`libs/gpu`)

Projetamos o subsistema gráfico clean-room desacoplado de bibliotecas gráficas nativas do host:
- **Banco de Registradores com Allowlist**: Mapeamento seguro das faixas conhecidas da arquitetura NV2A (`PMC`, `PBUS`, `PFIFO`, `PGRAPH`, `PCRTC`). Acessos fora da allowlist são descartados de maneira segura sem provocar falhas catastróficas.
- **Superfície RGBA8 Bounded**: Estrutura `GpuSurface` com buffer contíguo de pixels em formato RGBA8 (32 bits por pixel), validação de dimensões e numeração atômica de versão de snapshot (`snapshot_version`).
- **Decodificador e Processador de Pushbuffer**:
  - `Nv2aPushbufferDecoder`: Decodifica fluxos de comandos de pushbuffer em métodos, subcanais e pacotes com ou sem incremento.
  - `Nv2aPushbufferProcessor`: Executa comandos 2D determinísticos em CPU (`BIND_SURFACE`, `CLEAR`, `FILL_RECT`, `FLIP_SURFACE`), aplicando checagem de limites geométricos contra as dimensões da superfície.
- **Orçamentos Rígidos de Execução**: Limites estritos de palavras decodificadas, pacotes, métodos e ciclos virtuais por despacho, prevenindo negação de serviço e loops infinitos.
- **Interrupções de GPU**: O objeto `GpuInterruptSource` gera interrupções determinísticas de display integradas ao barramento e scheduler.

---

## 4. Integração na Sessão de Máquina (`libs/machine`)

A classe `MachineSession` foi estendida para incorporar o barramento PCI e o dispositivo NV2A:
- O barramento PCI é integrado ao barramento mestre da máquina.
- Os BARs da GPU NV2A (BAR 0 para registradores MMIO e BAR 1 para memória de vídeo/framebuffer) são registrados no espaço físico do sistema.
- A GPU é conectada como fonte de interrupções (`GpuInterruptSource`).
- O ciclo de vida da máquina expõe métodos para processamento determinístico de pushbuffers e extração de snapshots de quadros.

---

## 5. ABI C 1.3 e Shell Desktop React (`libs/c_api` e `apps/desktop`)

- **ABI C 1.3**:
  - Adicionamos a capacidade `XBLOB_CAPABILITY_GPU_FRAMEBUFFER`.
  - Funções exportadas: `xblob_machine_session_has_gpu_frame`, `xblob_machine_session_get_gpu_frame_metadata` e `xblob_machine_session_copy_gpu_frame` (padrão two-call com proteção contra buffer insuficiente).
- **Adaptador Rust em Tauri v2**:
  - Wrapper seguro RAII `SafeMachineSession` gerenciando ciclo de vida e alocação de buffers.
  - Comando IPC `get_diagnostic_frame_snapshot` codificando pixels RGBA8 em Base64 seguro para transferência ao frontend.
- **Interface React 19**:
  - Aba de inspeção "Framebuffer NV2A" com componente acessível `DiagnosticCanvasPreview`.
  - Renderização via Canvas 2D nativo do navegador.
  - Verificação de elegibilidade: mídias comerciais são identificadas com aviso claro de que o suporte gráfico está planejado e ainda não executa neste marco, enquanto fixtures sintéticas de validação têm seu preview renderizado interativamente.

---

## 6. Qualidade e Comprovação

Todas as verificações passaram com 100% de sucesso:
- CTest padrão: 19/19 testes aprovados.
- CTest ASan + UBSan: 19/19 testes aprovados (zero vazamentos de memória ou comportamentos indefinidos).
- `check-format.sh` e `clangd`: conformidade total, zero erros.
- `npm run typecheck && npm run lint && npm test && npm run build`: 30 testes React aprovados, zero warnings de linter, bundle gerado com sucesso.
- `npm run tauri -- --version`: `tauri-cli 2.11.4`.
- `cargo fmt --check && cargo clippy --locked -- -D warnings && cargo test --locked && cargo build --locked`: 7 testes Rust aprovados, zero warnings.
- Auditoria de arquitetura: Todos os arquivos mantêm-se estritamente abaixo do limite de 500 linhas.
