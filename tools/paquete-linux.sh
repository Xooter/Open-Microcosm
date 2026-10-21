#!/usr/bin/env bash
# Paquete de Linux portable: compila en un contenedor con glibc 2.35 (Ubuntu 22.04) y arma el
# .tar.gz en build/dist-portable. Requiere Docker. Reusa las fuentes de las dependencias ya
# descargadas por el preset release (build/release/_deps) si existen.
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -q -t openmicrocosm-linux-portable -f tools/docker/linux-portable.Dockerfile tools/docker >/dev/null
deps=()
for d in raylib entt imgui implot; do
    [ -d "build/release/_deps/$d-src" ] && deps+=("-DFETCHCONTENT_SOURCE_DIR_${d^^}=/src/build/release/_deps/$d-src")
done
docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD":/src -w /src openmicrocosm-linux-portable bash -c "
    cmake -S . -B build/dist-portable -G Ninja -DCMAKE_BUILD_TYPE=Release -DBIO_NATIVE=OFF -DBIO_ARCH=x86-64-v2 \
          -DBIO_BUILD_TESTS=OFF -DBIO_STATIC_RUNTIME=ON ${deps[*]} >/dev/null &&
    cmake --build build/dist-portable &&
    cd build/dist-portable && cpack"
