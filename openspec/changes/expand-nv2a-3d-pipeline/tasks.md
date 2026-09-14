## 1. Estado e comandos
- [x] 1.1 Definir render/vertex/texture state e unsupported metadata bounded.
- [x] 1.2 Expandir class/method dispatch 3D e validação transacional/budgets.

## 2. Pipeline
- [x] 2.1 Implementar viewport/scissor/cull e primitive assembly triangle list.
- [x] 2.2 Implementar vertex/index fetch com formats/stride/ranges prioritários.
- [x] 2.3 Implementar rasterização barycentric determinística em RGBA8.
- [x] 2.4 Implementar depth compare/write e color write masks.
- [x] 2.5 Implementar blend alpha básico configurável.
- [x] 2.6 Implementar texturas 2D linear/swizzled básicas, clamp/wrap e nearest.
- [x] 2.7 Integrar draws/flip ao pushbuffer e IRQ existente.

## 3. Qualidade
- [x] 3.1 Criar golden fixtures sintéticas para geometry/depth/blend/texture.
- [x] 3.2 Testar malformed/out-of-range/NaN/budgets e duas execuções.
- [x] 3.3 Executar testes GPU normal/sanitizers, format e clangd.
- [x] 3.4 Atualizar GPU README/diário, validar strict/ownership e commit focado.