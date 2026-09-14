# xblob USB Subsystem (`libs/usb`)

O subsistema USB implementa a especificação mínima de host controller OHCI (Open Host Controller Interface) compatível com a arquitetura original do Xbox e contratos limpos para dispositivos USB convidados (`UsbDevice`).

## Objetivos e Arquitetura

1. **OHCI Mínimo Determinístico**:
   - Registro de controle operacional (`HcControl`, `HcCommandStatus`, `HcInterruptStatus`, `HcInterruptEnable`, etc.).
   - Semântica estrita de W1C (Write-1-to-Clear) para interrupções e registradores de status de porta do Root Hub.
   - Master Interrupt Enable (`MIE`, bit 31) e sinalização determinística de IRQ.

2. **Travessia Delimitada de Descritores (ED/TD)**:
   - Validação preventiva de alinhamento em 16 bytes (`kDescriptorAlignment = 16`).
   - Verificação rigorosa de limites de memória guest (`UsbGuestMemory`).
   - Detecção de ciclos em listas de EDs e filas de TDs com orçamentos estritos (`kMaxEdTraversalBudget`, `kMaxTdTraversalBudget`).
   - Atomicidade transacional: nenhuma mutação parcial de estado guest ocorre em caso de falha de validação.

3. **Contrato de Dispositivo USB (`UsbDevice`)**:
   - Interface pura independente de APIs da plataforma host.
   - Suporte a transferências de controle (Setup, Data, Status) e transferências de interrupção periódicas.
   - Conexão e desconexão com atualização correspondente das portas do Root Hub.

4. **Isolamento de Memória**:
   - Acesso via abstração de memória virtual/física guest (`GuestAddr`), sem vazamento de ponteiros diretos da plataforma host.
