# Diário de Bordo: Pipeline 3D NV2A de Referência e Rasterização Determinística

**Data**: 14 de setembro de 2026  
**Contexto**: Implementação integral da child change OpenSpec `expand-nv2a-3d-pipeline` (13 tarefas).

---

## 1. Visão Geral e Arquitetura

Neste marco, expandimos as fundações gráficas da GPU NV2A (`libs/gpu`) para implementar um pipeline 3D de referência geral, limpo (*clean-room*) e determinístico executado inteiramente em CPU:
- **Ausência de Hacks**: Sem detecção de títulos, Title IDs, hashes ou nomes de arquivos. A implementação é genérica e orientada à especificação da arquitetura NV2A (NVIDIA Kelvin NV097 / NV096).
- **Isolamento de Memória e Transacionalidade**: Todas as mutações de renderização e acessos à memória física do convidado são validados previamente antes de qualquer escrita no buffer de cor (`GpuSurface`) ou no buffer de profundidade (`Nv2a3dContext`). Dados truncados, coordenadas inválidas ou NaN/Inf resultam em rejeição atômica e registro de falhas determinísticas (`GpuFault`).
- **Orçamentos Rígidos**: Limites de contagem de vértices processados (`max_vertices`) e triângulos rasterizados (`max_triangles`) foram adicionados aos orçamentos existentes de palavras, pacotes, métodos e jumps cíclicos.
- **Diagnósticos Bounded**: Métodos 3D desconhecidos ou ainda não implementados são registrados em um buffer de diagnósticos limitado (`kMaxUnsupportedDiagnosticEntries = 64`), permitindo auditoria sem risco de exaustão de memória.

---

## 2. Componentes Implementados

### 2.1 Tipos e Constantes 3D (`xblob/gpu/nv2a_3d_types.hpp`)
- Definição de constantes de métodos NV097 Kelvin 3D (`kClassNv097Kelvin3D`, `kMethodSetObject`, `kMethod3dClearSurface`, `kMethod3dViewport...`, `kMethod3dBlend...`, `kMethod3dDrawArrays`, `kMethod3dDrawElements...`).
- Enums para primitivas (`PrimitiveType::Triangles`, `TriangleStrip`, `TriangleFan`), culling (`CullMode::None`, `CW`, `CCW`), comparação de profundidade (`DepthFunc`), blending (`BlendFactor`, `BlendEquation`), formatos de atributos e texturas.

### 2.2 Amostrador de Texturas (`xblob/gpu/texture_sampler.hpp` / `.cpp`)
- Suporte a layouts Linear e Swizzled (ordem Morton 2D bit-interleaved para potências de dois).
- Formatos de textura: `R8G8B8A8`, `A8R8G8B8`, `X8R8G8B8`, `R5G6B5`.
- Modos de endereçamento: `Wrap` e `ClampToEdge`.
- Amostragem `Nearest` normalizada com proteção estrita de limites de leitura.

### 2.3 Fetch de Vértices e Índices (`xblob/gpu/vertex_fetch.hpp` / `.cpp`)
- Leitura transacional e segura de buffers de memória via `ReadWordFn`.
- Formatos de atributos suportados: `Float1`, `Float2`, `Float3`, `Float4`, `Ubyte4`.
- Índices de 16 e 32 bits com verificação de limites.
- Montagem determinística de primitivas (`Triangles`, `TriangleStrip`, `TriangleFan`).
- Validação preventiva com rejeição imediata de coordenadas NaN, Inf e overflow.

### 2.4 Rasterizador Determinístico (`xblob/gpu/rasterizer.hpp` / `.cpp`)
- Avaliação de funções de aresta com aritmética de ponto fixo 28.4 e aplicação estrita da regra de preenchimento *top-left*.
- Interpolação de atributos de vértices (cores RGBA e coordenadas de textura UV) e profundidade $Z$ corrigida por perspectiva via $1/W$.
- Teste de profundidade configurável (`Less`, `LessEqual`, `Greater`, etc.) com depth buffer dedicado de 32-bit float.
- Alpha blending configurável com normalização em ponto flutuante e máscara de escrita de canais de cor (`ColorMask`).

### 2.5 Contexto 3D NV2A (`xblob/gpu/nv2a_3d_context.hpp` / `.cpp`)
- Gerencia estado completo do pipeline (viewport, scissor, culling, blending, depth, atributos e textura stages).
- Alocação e gerenciamento do depth buffer associado à geometria da superfície ativa.
- Histórico limitado de diagnósticos de métodos não suportados.

### 2.6 Despacho de Métodos Kelvin e Integração ao Pushbuffer (`pushbuffer_processor.cpp` / `pushbuffer_3d_methods.cpp`)
- Mapeamento de subcanais associados a classes (`SetObject`).
- Despacho de métodos 3D Kelvin e validação transacional antes da mutação de superfícies.
- Verificação de orçamentos combinados de desenho e saltos.

---

## 3. Qualidade e Verificação

- **Testes Unitários Sintéticos 3D** (`tests/unit/test_gpu_3d.hpp` e `tests/unit/test_gpu.cpp`):
  1. `AllowlistAndDiagnostics`: Validação de classes permitidas e bounded diagnostics para métodos desconhecidos.
  2. `VertexIndexFetch`: Leitura de atributos, indexação 16/32-bit e rejeição de coordenadas hostis (NaN/Inf).
  3. `RasterizerDeterministicTriangles`: Verificação de golden pixels e diagonal compartilhada sem gaps ou overdraw.
  4. `DepthAndScissor`: Verificação de ordenação de profundidade e restrição geométrica por scissor box.
  5. `TextureSamplingLinearAndSwizzled`: Cobertura de formatos de pixel, decodificação Morton swizzled e modos de endereçamento.
  6. `AlphaBlendingAndColorMask`: Validação de combinação de cores e máscaras de escrita de canais.
  7. `Pushbuffer3dEndToEnd`: Fluxo completo de pushbuffer com flip de superfície e geração de IRQ.
- **Resultados**: 100% de aprovação (20/20 suítes no ctest, 13/13 testes unitários na suíte GPU), tanto em compilação padrão quanto com sanitizadores ASan/UBSan ativados.
- **Modularidade**: Todos os arquivos de código mantidos estritamente abaixo de 500 linhas.
