## Context

O projeto possui memória física/address space, CPU inicial, scheduler, parsers XBE e ABI/UI de inspeção. Ainda não existe tradução virtual, barramento próprio, loader transacional nem composição de máquina. Entradas permanecem hostis; somente fixtures sintéticas podem ser executadas em testes.

## Goals / Non-Goals

**Goals:**
- Conectar formatos, memória e CPU por contratos modulares.
- Implementar paginação IA-32 básica de 4 KiB e faults reproduzíveis.
- Preparar XBE sintético atomicamente e permitir steps controlados.
- Expor apenas diagnóstico seguro à ABI/UI.

**Non-Goals:**
- Kernel HLE, imports reais, TLS completo, interrupções, PAE/PSE, BIOS, GPU/áudio ou jogos.
- Alegar boot de XBE comercial.

## Decisions

### Módulos direcionais

Criar `libs/bus` dependente de common; ampliar memory com tradutor dependente de address space; criar `libs/loader` dependente de common/formats/memory; criar composição fina em `libs/machine` dependente de core/cpu/memory/bus/loader. Nenhum módulo inferior conhece machine/UI.

### MMU separada da RAM

`VirtualMemory` contém CR0/CR3/CPL mínimos e traduz por leituras do address space. PDE/PTE são decodificadas campo a campo, com aritmética unsigned. TLB simples é opcional, mas invalidação e comportamento devem ser testados. Apenas páginas 4 KiB sem PAE/PSE.

### Loader em duas fases

Primeiro produzir plano imutável de mappings/cópias/zero-fill/permissões validando integralmente arquivo e conflitos. Depois aplicar em nova sessão ou transação reversível. Entry point e chaves XBE usam fórmulas publicamente documentadas e fixtures sintéticas; mecanismos não suportados falham explicitamente.

### Bus como backend MMIO

Dispositivos implementam interface estreita de read/write. O bus valida ranges e roteia offsets; adaptador conecta-o ao MMIO do address space. Dispositivo de teste é nomeado synthetic/register-bank para não alegar hardware real.

### Sessão sem god object

`MachineSession` apenas possui/coordena subsistemas e delega trabalho; políticas ficam em loader/MMU/CPU/scheduler. Lifecycle usa enum e Result. Preparar não executa; step/run exige chamada explícita e somente testes sintéticos usarão execução neste marco.

### ABI/UI conservadoras

Adicionar capability e comando de “prepare diagnostics” apenas para fixture/formato suportado, sem Play. A UI pode mostrar validações, entry point e limitações. Nenhum endereço vira ponteiro ou dado mutável no frontend.

### Qualidade

Testes por módulo, rollback, faults/page bits e duas execuções determinísticas. Warnings como erros, ASan/UBSan, clangd, React/Rust existentes e CI três hosts permanecem gates. Atualizar diário e documentação honestamente.

## Risks / Trade-offs

- [Paginação incompleta diverge de hardware] → Escopo explícito 4 KiB, testes por bit e expansão posterior.
- [Loader corrompe estado em falha] → Plano em duas fases e aplicação em sessão nova/transação testada.
- [Machine vira god object] → Limitar a ownership/lifecycle e revisar tamanho/dependências.
- [Execução sintética interpretada como compatibilidade] → UI/docs sem Play e mensagens explícitas.
- [XBE encryption/address formulas erradas] → Fontes públicas documentadas, bytes sintéticos e erros conservadores.

## Migration Plan

1. Implementar/testar bus e adaptador MMIO.
2. Implementar/testar tradução virtual e faults.
3. Implementar plano/aplicação transacional do loader.
4. Criar sessão e integração sintética.
5. Estender ABI/UI somente com diagnóstico.
6. Executar todas as suítes, sanitizers, static checks, CI/docs/diário.