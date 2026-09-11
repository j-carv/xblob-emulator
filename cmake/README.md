# Infraestrutura de Compilação CMake (cmake/)

Este diretório contém módulos, configurações de compiladores e rotinas auxiliares para o sistema de compilação CMake do **xblob**.

## Arquivos e Módulos

- `CompilerWarnings.cmake`: Configuração padronizada e rigorosa de avisos de compilação (`-Wall`, `-Wextra`, `/W4`, etc.) mapeados por toolchain (Clang, GCC, MSVC) sem contaminação cruzada.
- `Sanitizers.cmake`: Ativação condicional de ferramentas de sanitização (AddressSanitizer, UndefinedBehaviorSanitizer) onde suportadas pelo compilador.
