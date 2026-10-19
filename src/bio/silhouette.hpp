#pragma once

#include "bio/cell.hpp"
#include "core/vec2.hpp"

#include <vector>

struct ProcessSpec;

// Silueta de una especie con prolongaciones en el cuerpo (cuernos de Ceratium, brazos de Staurastrum,
// espina caudal de Phacus): el contorno de la célula entera, con las prolongaciones unidas al cuerpo con
// un redondeo (como la teca o la pared real, que es una sola pieza), para el tamaño típico.
//
// Se arma una vez por especie: el campo de distancia del cuerpo (superelipse) se une con el de cada
// prolongación (una cápsula que se afina a lo largo de su eje curvo) con un mínimo suave; el contorno
// sale por marching squares y se remuestrea denso por longitud de arco. Determinista.
struct Silhouette {
    // Contorno denso (~0,5 µm entre puntos, parejo por longitud de arco), antihorario, centrado en el
    // promedio de sus puntos (el centro que usa el cuerpo blando), con el polo anterior del cuerpo
    // hacia +x. Vacío si la especie no tiene prolongaciones en el cuerpo.
    std::vector<Vec2> outline;
    float area = 0.0f;        // área encerrada [µm²]
    Vec2 bodyCenter;          // centro del cuerpo (de la superelipse) en ese marco
    float areaFactor = 1.0f;  // área de la silueta / área de la elipse del cuerpo (π·rx·ry)
};

Silhouette buildSilhouette(const CellSpec& body, const std::vector<ProcessSpec>& processes);

// Reparte `n` puntos por longitud de arco sobre un contorno cerrado denso, escalado por `scale`,
// empezando en la fracción `start` ∈ [0, 1) del perímetro. Con `reverse`, lo recorre al revés.
void sampleOutline(const std::vector<Vec2>& dense, int n, float scale, float start, bool reverse,
                   std::vector<Vec2>& out);

// Perímetro de un contorno cerrado.
float outlinePerimeter(const std::vector<Vec2>& outline);
