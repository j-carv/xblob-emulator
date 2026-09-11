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
3. **Barramento PCI (Peripheral Component Interconnect)**:
   - *PCI Local Bus Specification* (Revisão 2.2/2.3, PCI-SIG).
   - Mapeamento padrão de cabeçalho de configuração Type 0 (Vendor ID, Device ID, Command, Status, BARs 0..5, Interrupt Line/Pin).
   - Protocolo padrão de dimensionamento de BARs (`0xFFFFFFFF` write followed by read) e resolução determinística de colisões de faixas de endereçamento.
4. **GPU NV2A e Protocolo Pushbuffer**:
   - Documentações públicas de código aberto de arquitetura gráfica NV2A / GeForce 3 / Xbox (nouveau, especificações abertas de registradores NV20/NV2A).
   - Protocolo de comandos Pushbuffer (métodos, subcanais, pacotes Method/Non-Inc) derivados estritamente de especificações públicas.
   - Desacoplamento total de APIs gráficas proprietárias e isolamento em superfícies RGBA8 puras em memória de host.
5. **Sistema de Arquivos XDVDFS e Mídia Xbox**:
   - Especificações públicas de sistemas de arquivos de disco do console Xbox original (documentação de comunidade, XDVDFS/XISO layout specs).
   - Estrutura de setores (2048 bytes), magic descriptor `"MICROSOFT*XBOX*MEDIA"` nos setores 32 (raw) ou 0 (trimmed).
   - Estrutura da tabela de diretórios em árvore binária (BST com offset esquerdo/direito relativos em palavras de 4 bytes, 14 bytes de cabeçalho por entrada, nomes ASCII sem nulos intermediários).
6. **Semântica de VFS Xbox e Serviços de Arquivo NT/Kernel**:
   - Especificações da API NT e documentações públicas de ordinais de exportação do kernel do Xbox (NtCreateFile 190, NtReadFile 219, NtWriteFile 256, SetFilePointer 224, NtClose 18, NtQueryInformationFile 217, NtQueryDirectoryFile 216, NtDeviceIoControlFile 196).
   - Semântica de normalização de caminhos (drive letters como `D:`, separadores de barra invertida, case-insensibilidade, ausência de path traversal `..`).

---

## 3. Isolamento Arquitetural de Segurança

- **Ponteiros Host vs. Guest**: Nenhuma estrutura de dados ou operando da CPU armazena ou expõe ponteiros nativos do host. Todos os acessos a memória passam pelo tradutor de espaço virtual (`VirtualMemory` / `AddressSpace`).
- **Exceções Arquiteturais**: As faltas `#UD` (Invalid Opcode), `#GP` (General Protection Fault) e `#PF` (Page Fault) são tratadas estritamente segundo as especificações do manual IA-32, distinguindo-se categoricamente de erros internos da máquina emuladora.
- **Kernel HLE e Registro Allowlist**: Apenas ordinais expressamente registrados na tabela de despacho (`ExportRegistry`) são aceitos. Tentativas de chamada a ordinais desconhecidos resultam em falha segura e controlada sem invocar código arbitrário.
- **Registradores e Comandos NV2A em Allowlist**: O registrador de GPU e os métodos de pushbuffer rejeitam terminantemente qualquer comando ou endereço fora das faixas explicitamente permitidas, descartando operações malformadas com códigos de erro defensivos.
- **Orçamentos Rígidos de Execução**: Pushbuffers e instruções são delimitados por orçamentos máximos de contagem de métodos, palavras e ciclos, impedindo loops infinitos e negação de serviço.
