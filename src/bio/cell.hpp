#pragma once

#include "core/rng.hpp"
#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <entt/entity/registry.hpp>

#include <cstdint>

struct CellSpec {
    Vec2 center;
    float radiusX = 40.0f, radiusY = 25.0f; // semiejes [µm]
    float angle = 0.0f;                     // [rad]
    float shapeRate = 8.0f; // memoria de forma [1/s]: película rígida (Paramecium) ~8,
                            // córtex blando (Amoeba) ~0.3
    float nucleusFraction = 0.32f;          // radio del núcleo / semieje menor
    std::uint8_t r = 180, g = 185, b = 160; // tinte del citoplasma
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
