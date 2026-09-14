## 1. Decoder e diagnóstico
- [x] 1.1 Adicionar prefixes/forms necessários e metadata unsupported-form sem regressão.
- [x] 1.2 Testar truncamento, 15-byte limit, prefix conflicts e EIP/estado atômico.

## 2. Instruções
- [x] 2.1 Implementar SHL/SHR/SAR/ROL/ROR counts 1/imm8/CL e flags/borders.
- [x] 2.2 Implementar MUL/IMUL forms prioritárias com EDX:EAX e flags.
- [x] 2.3 Implementar DIV/IDIV 32-bit com #DE zero/overflow sem UB/mutação parcial.
- [x] 2.4 Implementar MOVZX/MOVSX/CDQ/BT/SETcc forms prioritárias.
- [x] 2.5 Implementar MOVS/STOS/LODS/CMPS/SCAS byte/dword e DF.
- [x] 2.6 Implementar REP/REPE/REPNE em micro-steps bounded e retomáveis.
- [x] 2.7 Implementar XCHG/CMPXCHG básicos no modelo single-core.

## 3. Qualidade da lane
- [x] 3.1 Adicionar vetores públicos de flags/limites e faults de memória por família.
- [x] 3.2 Executar build e CTest CPU normal/sanitizers, format e clangd.
- [x] 3.3 Atualizar CPU README e diário da lane sem tocar docs globais.
- [x] 3.4 Validar OpenSpec strict, ownership e criar commit focado da lane.