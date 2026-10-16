#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Paquetes de contenido cargados (fase 20.0): su manifiesto ([paquete] de paquete.toml) y una
// huella de sus archivos, para la procedencia de una corrida y para avisar, al cargar una escena,
// qué paquete le falta. El de fábrica es "base" (assets/content/base/).
struct PackInfo {
    std::string id, name, version, author, license;
    std::string dir;              // carpeta de donde se cargó
    std::uint64_t fingerprint = 0; // FNV-1a de los nombres y el contenido de sus .toml, en orden
};

const std::vector<PackInfo>& loadedPacks();
const PackInfo* findPack(const std::string& id);
std::string fingerprintHex(std::uint64_t fingerprint);
