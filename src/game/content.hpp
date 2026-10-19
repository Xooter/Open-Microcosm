#pragma once

#include "bio/species.hpp"
#include "game/biome.hpp"
#include "game/mineral.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Paquetes de contenido (fase 20.0; formato en docs/contenido.md). Un paquete es una carpeta con
// `pack.toml` y una carpeta por tipo con un archivo por definición (core/pack_files:
// beings/paramecium.toml, con la fisiología del ser en el mismo archivo; biomes/, minerals/,
// body_plans/, media/, vessels/, plates/, lessons/, reagents/, glossary/), o un solo archivo
// `.biopack`: un TOML con `[pack]` y todas sus definiciones (`[[being]]`, `[[body_plan]]`,
// `[[mineral]]`, `[[biome]]`, `[physiology.<id del ser>]`, `[[medium]]`, `[[vessel]]`, `[[plate]]`,
// `[[scene]]`, `[[lesson]]`, `[[reagent]]`, `[[term]]`). Son solo datos: nada ejecutable, con
// tamaños y cantidades acotados. El de fábrica es "base" (assets/content/base/); los del usuario
// viven en <datos>/biogame/packs/ y se cargan al empezar.
struct PackInfo {
    std::string id, name, version, author, license;
    std::string path;              // carpeta o archivo de donde se cargó
    std::uint64_t fingerprint = 0; // FNV-1a de sus archivos (nombres y contenido)
};

// Los cargados, en orden de carga (el de fábrica primero). Las referencias siguen valiendo al
// cargar más.
std::vector<PackInfo> loadedPacks();
const PackInfo* findPack(const std::string& id);
std::string fingerprintHex(std::uint64_t fingerprint);

// Carga un paquete (carpeta o .biopack). Lo que no se pudo cargar queda en `messages`; devuelve
// false si se rechaza entero (manifiesto inválido, ya cargado, demasiado grande, programa viejo).
// Las escenas que trae se copian a la carpeta de placas (<datos>/biogame/plates).
bool loadPack(const std::string& path, std::vector<std::string>& messages);
std::string userPacksDir();
// Todos los paquetes de la carpeta del usuario, en orden alfabético.
void loadUserPacks(std::vector<std::string>& messages);
// Copia un .biopack a la carpeta del usuario y lo carga (queda para las próximas sesiones).
bool importPack(const std::string& file, std::vector<std::string>& messages);

// Qué va en un .biopack exportado: el manifiesto y las definiciones elegidas (cada una como se
// leyó, con su plan corporal si no es de fábrica y su fisiología), más escenas (nombre, texto).
struct BiopackSpec {
    std::string id, name, version = "1", author, license = "GPL-3.0-or-later";
    std::vector<SpeciesId> beings;
    std::vector<MineralKind> minerals;
    std::vector<BiomeId> biomes;
    std::vector<std::string> prepared; // ids de placas preparadas (con su medio y su recipiente)
    std::vector<std::pair<std::string, std::string>> scenes;
};
// El texto del .biopack (vacío si no se puede armar; el motivo va a `messages`).
std::string biopackText(const BiopackSpec& spec, std::vector<std::string>& messages);

// Avisos de todos los registros (seres, planes, fisiología, minerales, biomas) y de los paquetes.
std::vector<std::string> contentWarnings();

// Límites de un paquete (seguridad: un archivo compartido no puede colgar el programa).
inline constexpr std::size_t kMaxPackBytes = 8u << 20; // 8 MB
inline constexpr std::size_t kMaxPackIdLength = 64;
inline constexpr std::size_t kMaxPackFiles = 4096;
