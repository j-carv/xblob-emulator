## 1. Diagnóstico e infraestrutura
- [ ] 1.1 Definir metadata local de unsupported export e registry stats bounded.
- [ ] 1.2 Refatorar handlers por domínio mantendo arquivos <500 linhas e dependências permitidas.

## 2. Serviços
- [ ] 2.1 Implementar virtual memory allocate/free/query e protection checks.
- [ ] 2.2 Expandir heap APIs e validações de alignment/range/double-free.
- [ ] 2.3 Implementar tick/system/performance time determinísticos.
- [ ] 2.4 Implementar timers one-shot/periodic e cancelamento sem eventos órfãos.
- [ ] 2.5 Expandir thread create/exit/yield e lifecycle de stacks/contextos.
- [ ] 2.6 Implementar semaphore/mutant e waits single/multiple com timeout/wake FIFO.
- [ ] 2.7 Ampliar file/device I/O síncrono prioritário e unsupported async honesto.

## 3. Qualidade da lane
- [ ] 3.1 Testar invalid guest pointers, stale handles, timeout, cancellation e exhaustion.
- [ ] 3.2 Testar determinismo de timers/threads/waits em duas execuções.
- [ ] 3.3 Executar build e CTest kernel normal/sanitizers, format e clangd.
- [ ] 3.4 Atualizar Kernel README e diário da lane sem tocar docs globais.
- [ ] 3.5 Validar OpenSpec strict, ownership e criar commit focado da lane.