#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==> Verificando formato de código C/C++..."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" &> /dev/null; then
    echo "AVISO: clang-format não encontrado. Pule a verificação se a ferramenta não estiver instalada."
    exit 0
fi

FAILED=0
FILES=$(find "${ROOT_DIR}/apps" "${ROOT_DIR}/libs" "${ROOT_DIR}/tests" -type f \( -name "*.hpp" -o -name "*.h" -o -name "*.cpp" -o -name "*.c" \) 2>/dev/null || true)

if [ -z "$FILES" ]; then
    echo "Nenhum arquivo C/C++ encontrado ainda para verificar."
    exit 0
fi

for f in $FILES; do
    if ! "$CLANG_FORMAT" --dry-run --Werror "$f" 2>&1; then
        echo "Erro de formatação em: $f"
        FAILED=1
    fi
done

if [ "$FAILED" -ne 0 ]; then
    echo "Erro: Formatação fora do padrão detectada. Execute 'clang-format -i <arquivos>' para corrigir."
    exit 1
fi

echo "==> Todos os arquivos C/C++ verificados estão em conformidade com o padrão."
