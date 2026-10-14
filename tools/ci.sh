#!/usr/bin/env bash
# CI local (MVP: estabilidad). Sin remoto todavía: correr antes de cada commit importante.
#
#   tools/ci.sh            compila release sin warnings, tests rápidos y una prueba larga corta
#   tools/ci.sh --todo     además los tests largos y la validación científica
#   tools/ci.sh --paquetes además arma los paquetes de Linux (dist) y Windows (MinGW)
set -euo pipefail
cd "$(dirname "$0")/.."

todo=0
paquetes=0
for arg in "$@"; do
    case "$arg" in
    --todo) todo=1 ;;
    --paquetes) paquetes=1 ;;
    *) echo "uso: tools/ci.sh [--todo] [--paquetes]" >&2; exit 1 ;;
    esac
done

paso() { printf '\n== %s\n' "$1"; }

# Compila un preset y falla si hay warnings de nuestro código (los de dependencias no cuentan).
compilar() {
    local preset=$1 log
    log=$(mktemp)
    cmake --preset "$preset" >/dev/null
    if ! cmake --build --preset "$preset" 2>&1 | tee "$log" >/dev/null; then
        grep -E "error" "$log" | head -20
        return 1
    fi
    if grep -E "warning:" "$log" | grep -v "/_deps/" | grep -q .; then
        grep -E "warning:" "$log" | grep -v "/_deps/" | head -20
        echo "hay warnings en $preset" >&2
        return 1
    fi
}

paso "compilar release"
compilar release

paso "tests rápidos"
ctest --test-dir build/release -L rapidos --output-on-failure

paso "prueba larga corta (finitos, balance de masa, memoria)"
./build/release/bio_bench --threads 4 --wildlife 1 --steps 1200 --warmup 0 --soak 300

if [ "$todo" = 1 ]; then
    paso "tests largos y validación"
    ctest --test-dir build/release -L "largos|validacion" --output-on-failure
fi

if [ "$paquetes" = 1 ]; then
    paso "paquete Linux portable (Docker, glibc 2.35)"
    tools/paquete-linux.sh
    paso "paquete Windows (MinGW)"
    compilar windows
    (cd build/windows && cpack)
fi

paso "listo"
