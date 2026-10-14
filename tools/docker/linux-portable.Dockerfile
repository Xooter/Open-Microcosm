# Entorno para el paquete de Linux (MVP: distribución). Se compila sobre una glibc vieja (Ubuntu
# 22.04: 2.35) para que el binario corra en las distribuciones actuales; compilado en la máquina de
# desarrollo pedía la glibc más nueva.
FROM ubuntu:22.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        g++-12 ninja-build git python3-pip ca-certificates \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev \
    && pip3 install --no-cache-dir "cmake>=3.28,<4" \
    && rm -rf /var/lib/apt/lists/*
ENV CC=gcc-12 CXX=g++-12
