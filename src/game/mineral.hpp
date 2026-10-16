#pragma once

#include "sim/particles.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Minerales del sedimento como datos (fase 20.0): los de fábrica vienen de
// assets/content/base/minerales.toml; un paquete puede sumar los suyos. MineralKind es el índice
// en el registro (los nombres del enum son alias de los de fábrica, en su orden).
struct MineralInfo {
    std::string key, name, grainName, description, style, pack;
    std::string source; // el [[mineral]] como se leyó (para exportarlo)
    float radiusMin = 1.0f, radiusMax = 1.0f; // [µm]
    float densityGap = 1650.0f;               // densidad menos la del agua [kg/m³] (ley de Stokes)
    float opacity = 0.1f;                     // fracción de la luz que bloquea
    bool flocculates = false;                 // forma flóculos al sembrar el sedimento
};

int mineralCount();
// Fuera de rango devuelve el primero (sin el paquete de fábrica, una arcilla genérica).
const MineralInfo& mineralInfo(MineralKind kind);
const char* mineralName(MineralKind kind);
std::optional<MineralKind> findMineral(std::string_view key);
// Estilos de dibujo que el render sabe hacer (para validar los datos).
const std::vector<std::string>& mineralStyles();

// Carga los [[mineral]] de un archivo TOML de un paquete (se agregan a los cargados). Los que
// tienen errores se saltean y quedan en `mineralLoadWarnings`; false si el archivo no se lee.
bool loadMineralsFile(const std::string& path, const std::string& pack, std::string* error = nullptr);
bool loadMineralsText(const std::string& text, const std::string& source, const std::string& pack,
                      std::string* error = nullptr);
const std::vector<std::string>& mineralLoadWarnings();
