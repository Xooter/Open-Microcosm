#pragma once

#include "core/vec2.hpp"
#include "game/inspect.hpp"

#include <cstdint>
#include <string>
#include <vector>

class World;

// Escala biológica que se está mirando, según cuánto mundo entra en la vista (no según el
// porcentaje de zoom): con la placa entera se ven poblaciones; con decenas de µm, células; con
// pocos µm, sus estructuras; con menos de ~1 µm, lo subcelular. Cada ser elige además su propio
// nivel de detalle por su tamaño en pantalla (render/lod.hpp); esto es solo para orientarse.
enum class BioScale : std::uint8_t { Environment, Organisms, Cells, CellStructures, Subcellular };

// `viewMicrons`: el lado menor de la vista [µm].
BioScale bioScaleFor(float viewMicrons);
const char* bioScaleName(BioScale scale);

// Miga de pan: Placa > organismo > célula > estructura. Sigue al ser elegido si está a la vista;
// si no, a lo que está en el centro. Sin raylib: la arma la app y la prueban los tests.
struct ScalePath {
    BioScale scale = BioScale::Environment;
    std::vector<std::string> crumbs; // ASCII; la UI traduce el primero
};

ScalePath scalePath(const World& world, Vec2 center, float viewMicrons, const Picked& focus);

// Largo "redondo" (1, 2 o 5 × 10ⁿ µm) cercano a `target`, y su texto con la unidad que le
// corresponde (mm, µm o nm). Lo usa la barra de escala.
float niceLength(float target);
std::string formatLength(float microns);
