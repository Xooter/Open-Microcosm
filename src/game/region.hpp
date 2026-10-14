#pragma once

#include "bio/species.hpp"
#include "core/vec2.hpp"

#include <vector>

class World;

// Cuantificación por región (fase 13): lo que hay dentro de un polígono dibujado sobre la placa.
// Las células cuentan por su centroide; las concentraciones son la media de las celdas de agua
// cuyo centro cae adentro.
struct RegionStats {
    bool valid = false;
    float area = 0.0f;       // [µm²]
    int cells = 0;           // individuos (sin partes de cuerpos compuestos ni granos)
    int species[kSpeciesCount] = {};
    int bacteria = 0;
    float biomass = 0.0f;    // [pmol] de las células
    int waterCells = 0;      // celdas de la grilla química promediadas
    float oxygen = 0.0f, ph = 0.0f, phosphate = 0.0f, nitrogen = 0.0f, silicate = 0.0f, iron = 0.0f,
          vitamin = 0.0f, organic = 0.0f; // [µM] (pH sin unidad)
    float cellDensity() const { return area > 0.0f ? cells / area * 1e6f : 0.0f; } // [células/mm²]
};

// Área de un polígono simple [µm²] (fórmula del lazo, sin signo).
float polygonArea(const std::vector<Vec2>& polygon);
// Largo de una polilínea [µm].
float polylineLength(const std::vector<Vec2>& points);

RegionStats quantifyRegion(const World& world, const std::vector<Vec2>& polygon);
