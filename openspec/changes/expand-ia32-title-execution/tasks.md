## 1. Decoder e diagnóstico
- [ ] 1.1 Adicionar prefixes/forms necessários e metadata unsupported-form sem regressão.
- [ ] 1.2 Testar truncamento, 15-byte limit, prefix conflicts e EIP/estado atômico.

## 2. Instruções
- [ ] 2.1 Implementar SHL/SHR/SAR/ROL/ROR counts 1/imm8/CL e flags/borders.
- [ ] 2.2 Implementar MUL/IMUL forms prioritárias com EDX:EAX e flags.
- [ ] 2.3 Implementar DIV/IDIV 32-bit com #DE zero/overflow sem UB/mutação parcial.
- [ ] 2.4 Implementar MOVZX/MOVSX/CDQ/BT/SETcc forms prioritárias.
- [ ] 2.5 Implementar MOVS/STOS/LODS/CMPS/SCAS byte/dword e DF.
- [ ] 2.6 Implementar REP/REPE/REPNE em micro-steps bounded e retomáveis.
- [ ] 2.7 Implementar XCHG/CMPXCHG básicos no modelo single-core.

## 3. Qualidade da lane
- [ ] 3.1 Adicionar vetores públicos de flags/limites e faults de memória por família.
- [ ] 3.2 Executar build e CTest CPU normal/sanitizers, format e clangd.
- [ ] 3.3 Atualizar CPU README e diário da lane sem tocar docs globais.
- [ ] 3.4 Validar OpenSpec strict, ownership e criar commit focado da lane.