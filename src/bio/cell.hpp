#pragma once

#include "core/rng.hpp"
#include "core/vec2.hpp"
#include "sim/body_particles.hpp"
#include "sim/taper.hpp"

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct CellSpec {
    Vec2 center;
    float radiusX = 40.0f, radiusY = 25.0f; // semiejes [µm]
    float angle = 0.0f;                     // [rad]
    float shapeRate = 8.0f; // memoria de forma [1/s]: película rígida (Paramecium) ~8,
                            // córtex blando (Amoeba) ~0.3
    float nucleusFraction = 0.32f;          // radio del núcleo / semieje menor
    float squareness = 2.0f;                // superelipse: 2 = elipse, ~6 = cilindro (Spirogyra)
    Taper taper;                            // afinamiento de los polos (sim/taper.hpp)
    float bend = 0.0f;                      // arco del eje [rad] (media luna de Closterium; solo la silueta)
    // Gránulos libres en el citoplasma. El espermatozoide maduro casi no tiene citoplasma (lo pierde en la
    // gota citoplasmática): su cabeza es el núcleo y el acrosoma.
    bool granules = true;
    std::uint8_t r = 180, g = 185, b = 160; // tinte del citoplasma
    // Con prolongaciones en el cuerpo, el contorno de la especie (bio/silhouette, para el tamaño típico)
    // reemplaza a la superelipse: `outlineScale` es el tamaño respecto del típico y `outlineBodyCenter`,
    // el centro del cuerpo en el marco del contorno (ahí van el núcleo y los gránulos).
    const std::vector<Vec2>* outline = nullptr;
    float outlineScale = 1.0f;
    Vec2 outlineBodyCenter;
};

// Componentes de célula (el cuerpo físico es el componente RingBody de sim/soft_body.hpp).
struct CellLook {
    std::uint8_t r, g, b;
};

struct CellAnatomy {
    std::uint32_t nucleus;          // slot del núcleo
    std::uint32_t contractileVacuole; // slot de la vacuola contráctil
};

inline constexpr float kMembraneSpacing = 3.0f; // [µm] entre partículas de membrana

entt::entity createCell(entt::registry& registry, BodyParticles& bodies, const CellSpec& spec,
                        Rng& rng);
void destroyCell(entt::registry& registry, BodyParticles& bodies, entt::entity cell);

// Célula genérica de prueba (fase 2): mitad con película rígida, mitad blandas.
CellSpec randomCellSpec(Rng& rng, Vec2 center);

// Punto de la superelipse de semiejes (rx, ry) y exponente p en el parámetro t.
// `taper` afina los polos (sim/taper.hpp).
inline Vec2 superellipsePoint(float rx, float ry, float p, float t, const Taper& taper = {}) {
    const float c = std::cos(t), s = std::sin(t);
    const float x = rx * std::copysign(std::pow(std::abs(c), 2.0f / p), c);
    return {x, ry * std::copysign(std::pow(std::abs(s), 2.0f / p), s) * taperFactor(taper, x, rx)};
}
