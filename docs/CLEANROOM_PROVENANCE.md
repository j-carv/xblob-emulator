# Proveniência Clean-Room e Especificações Públicas (docs/CLEANROOM_PROVENANCE.md)

Este documento atesta os princípios de engenharia reversa limpa (*clean-room*) e as fontes públicas utilizadas na concepção e implementação dos subsistemas de CPU IA-32 (`libs/cpu`) e Kernel HLE (`libs/kernel`) do projeto **xblob**.

---

## 1. Princípios de Engenharia Clean-Room

1. **Ausência Total de SDKs ou Headers Proprietários**:
   - Nenhum arquivo de cabeçalho (`.h`), biblioteca (`.lib`), ferramenta ou documentação restrita do Microsoft Xbox SDK oficial foi utilizado, consultado ou incluído no repositório.
2. **Ausência de Binários ou Firmwares Oficiais**:
   - Nenhuma imagem de BIOS de varejo (*retail*) ou desenvolvimento (*devkit*), dump de Boot ROM MCPX ou executável proprietário do sistema operacional (`xboxkrnl.exe`) foi descompilado ou incorporado.
3. **Fixtures Exclusivamente Sintéticas**:
   - Todas as baterias de testes unitários e de integração operam exclusivamente sobre fixtures sintéticas geradas programmaticamente em memória (`tests/fixtures/synthetic_media.cpp`).

---

## 2. Fontes Públicas e Especificações de Referência

O desenvolvimento apoia-se estritamente em documentações públicas e literatura técnica amplamente disponível:

1. **Arquitetura do Conjunto de Instruções IA-32 (x86)**:
   - *Intel® 64 and IA-32 Architectures Software Developer’s Manual*:
     - Volume 1: Basic Architecture (Registradores, GPRs, EFLAGS, Formato de Instruções).
     - Volume 2A, 2B, 2C, 2D: Instruction Set Reference (Opcodes, ModR/M, SIB, flags afetadas/preservadas, semântica transacional de instruções de ALU, Stack e Branch).
     - Volume 3A: System Programming Guide (Tabelas de descritores IDT, Interrupt/Trap Gates 32-bit, semântica de frame same-ring e flags de exceção #UD, #GP, #PF).
2. **Documentação Pública de Ordinais de Exportação**:
   - Especificações públicas de projetos open-source e documentações históricas de comunidade (como especificações do OpenXDK e referências públicas de ordinais de exportação do kernel).
   - Mapeamento ordinal puro baseado em convenções padrão `__stdcall` com checagem rigorosa de ponteiros guest.

---

## 3. Isolamento Arquitetural de Segurança

- **Ponteiros Host vs. Guest**: Nenhuma estrutura de dados ou operando da CPU armazena ou expõe ponteiros nativos do host. Todos os acessos a memória passam pelo tradutor de espaço virtual (`VirtualMemory` / `AddressSpace`).
- **Exceções Arquiteturais**: As faltas `#UD` (Invalid Opcode), `#GP` (General Protection Fault) e `#PF` (Page Fault) são tratadas estritamente segundo as especificações do manual IA-32, distinguindo-se categoricamente de erros internos da máquina emuladora.
- **Kernel HLE e Registro Allowlist**: Apenas ordinais expressamente registrados na tabela de despacho (`ExportRegistry`) são aceitos. Tentativas de chamada a ordinais desconhecidos resultam em falha segura e controlada sem invocar código arbitrário.
