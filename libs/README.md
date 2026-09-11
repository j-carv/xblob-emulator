# Bibliotecas do Núcleo (libs/)

Este diretório contém os componentes e subsistemas modulares do **xblob**.

## Regras de Modularidade e Dependência

1. **Unidirecionalidade**: Subsistemas de camadas inferiores nunca conhecem nem incluem bibliotecas de camadas superiores.
2. **Proibição de Ciclos**: Não são permitidas dependências circulares entre quaisquer bibliotecas.
3. **Sem Acesso Direto a APIs do Host no Domínio**: Primitivas específicas do sistema operacional (Win32, POSIX, Cocoa) ficam restritas a `libs/platform`.
4. **Isolamento de Erros**: O tratamento de erros de parsing e emulação usa tipos explícitos (`Result<T, E>`), sem propagação desenfreada de exceções.

## Mapa de Subsistemas

| Diretório | Status no Marco Atual | Responsabilidade Principal | Dependências Permitidas |
| :--- | :--- | :--- | :--- |
| `common/` | **Implementado** | Tipos base, `Result<T, E>`, códigos de erro, aritmética segura | Nenhuma (somente `<cstdint>`, etc.) |
| `io/` | **Implementado** | Visualizadores de bytes e leitura com cursor sem casts de struct | `common` |
| `formats/` | **Implementado** | Parsers defensivos de XBE, detecção de ISO/XISO e fachada `MediaInspector` | `io`, `common` |
| `platform/` | *Futuro* | Abstração de janelamento, áudio, relógio e threads do host | `common` |
| `memory/` | **Implementado** | Memória física 64/128 MiB, espaço 32 bits, permissões e MMIO | `common` |
| `core/` | **Implementado** | Scheduler determinístico, relógio virtual monotônico e fila de eventos | `common` |
| `cpu/` | **Implementado** | Intérprete IA-32 de referência, fetch seguro e ciclo de vida | `common`, `memory` |
| `gpu/` | *Futuro* | Emulação da GPU NV2A e pipeline gráfico | `common`, `memory` |
| `audio/` | *Futuro* | Emulação da APU MCPX (DSP de áudio e sintetizador de voz) | `common`, `memory` |
| `kernel/` | *Futuro* | HLE das chamadas do kernel e subsistemas nativos do Xbox | `common`, `memory`, `cpu` |
