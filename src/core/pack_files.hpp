#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

// Disposición de un paquete de contenido en carpeta: el manifiesto (`pack.toml`) y una carpeta
// por tipo de definición con un archivo por definición (beings/paramecium.toml). La usan los
// registros (para el paquete de fábrica) y game/content (para los del usuario).

inline constexpr const char* kPackManifest = "pack.toml";
// Las traducciones de los textos del paquete (lang/<código>.txt; ui/i18n).
inline constexpr const char* kPackLangFolder = "lang";

// Las carpetas en orden de dependencias: los seres nombran planes; los biomas, seres y minerales;
// las placas, medios, recipientes y seres; las lecciones, placas, biomas y seres.
inline constexpr const char* kPackFolders[] = {"body_plans", "minerals", "beings",  "biomes",  "media",
                                               "vessels",    "plates",   "lessons", "reagents", "glossary"};

// Los .toml de una carpeta del paquete, en su orden de carga: primero los que el manifiesto lista
// en `[pack.order]` (los índices de los seres, biomas y minerales de fábrica son su orden de
// carga: las escenas guardadas dependen de él), después el resto por nombre.
std::vector<std::filesystem::path> packFolderFiles(const std::filesystem::path& pack, std::string_view folder);

// La carpeta del paquete de fábrica (assets/content/base).
std::filesystem::path basePackDir();
