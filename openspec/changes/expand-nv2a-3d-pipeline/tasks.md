## 1. Estado e comandos
- [ ] 1.1 Definir render/vertex/texture state e unsupported metadata bounded.
- [ ] 1.2 Expandir class/method dispatch 3D e validação transacional/budgets.

## 2. Pipeline
- [ ] 2.1 Implementar viewport/scissor/cull e primitive assembly triangle list.
- [ ] 2.2 Implementar vertex/index fetch com formats/stride/ranges prioritários.
- [ ] 2.3 Implementar rasterização barycentric determinística em RGBA8.
- [ ] 2.4 Implementar depth compare/write e color write masks.
- [ ] 2.5 Implementar blend alpha básico configurável.
- [ ] 2.6 Implementar texturas 2D linear/swizzled básicas, clamp/wrap e nearest.
- [ ] 2.7 Integrar draws/flip ao pushbuffer e IRQ existente.

## 3. Qualidade
- [ ] 3.1 Criar golden fixtures sintéticas para geometry/depth/blend/texture.
- [ ] 3.2 Testar malformed/out-of-range/NaN/budgets e duas execuções.
- [ ] 3.3 Executar testes GPU normal/sanitizers, format e clangd.
- [ ] 3.4 Atualizar GPU README/diário, validar strict/ownership e commit focado.