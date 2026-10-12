#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>
#include <vector>

// Física sin inercia (bajo Reynolds): cada restricción relaja su error con una tasa [1/s],
// corrección = 1 − exp(−tasa · h). Es independiente del sub-paso h y da un comportamiento
// viscoelástico: la célula se deforma y vuelve a su forma sin rebotar.
struct SoftBodyTuning {
    int substeps = 10;
    float edgeRate = 3000.0f; // largo de la membrana (casi inextensible)
    float bendRate = 60.0f;   // flexión local (shape matching por ventanas)
    float areaRate = 3000.0f; // área = volumen de agua (incompresible a corto plazo)
    float shapeScale = 1.0f;  // multiplica la memoria de forma propia de cada cuerpo
    int iterations = 3; // restricciones + contactos por sub-paso
};

// Cuerpo blando: anillo de membrana + partículas interiores confinadas.
struct RingBody {
    std::uint32_t id = 0; // dueño de las partículas (para colisiones)

    std::vector<std::uint32_t> ring; // slots de membrana, en orden
    std::vector<Vec2> restRing;      // forma de reposo centrada, en el marco del cuerpo
    std::vector<float> restEdge;     // |r[i+1] − r[i]|
    float restArea = 0.0f;           // con signo (indica la orientación del anillo)
    float shapeRate = 0.0f;          // memoria de forma [1/s]: película rígida alta, ameba ~0

    // Interior: los primeros `organelleCount` son organelos (chocan y empujan la membrana);
    // el resto son gránulos que solo se confinan.
    std::vector<std::uint32_t> interior;
    std::vector<Vec2> restInterior;
    std::vector<float> anchorRate; // anclaje al citoesqueleto [1/s], 0 = libre
    std::uint32_t organelleCount = 0;

    // Estado derivado, actualizado por soft_body::refresh.
    std::vector<Vec2> poly;
    Vec2 centroid;
    float angle = 0.0f;
    Vec2 aabbMin, aabbMax;
};

namespace soft_body {

void predict(RingBody& body, BodyParticles& bp, float h);
void solve(RingBody& body, BodyParticles& bp, const SoftBodyTuning& tuning, float h);
void confineGranules(RingBody& body, BodyParticles& bp);
void refresh(RingBody& body, const BodyParticles& bp);
// Traslada el cuerpo entero si su centroide salió del mundo toroidal.
void wrap(RingBody& body, BodyParticles& bp, float width, float height);

float area(const RingBody& body, const BodyParticles& bp);
// Distancia RMS de la membrana a su forma de reposo alineada [µm].
float shapeError(const RingBody& body, const BodyParticles& bp);

} // namespace soft_body
