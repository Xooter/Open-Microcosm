#pragma once

#include "bio/feeding.hpp"
#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>
#include <vector>

struct RingBody;

// Componente: copa fagocítica en formación (Amoeba). Mecanismo de "cremallera": desde el
// punto de contacto, la membrana se adhiere a la superficie de la presa y la va forrando
// hacia el lado opuesto. Cuando los dos labios se tocan, las membranas se fusionan y la presa
// queda en un fagosoma.
struct PhagocyticCup {
    std::uint32_t preyId = 0;     // RingBody::id de la presa
    std::uint32_t anchorSlot = 0; // punto de membrana del primer contacto
    float front[2] = {0.0f, 0.0f}; // ángulo del frente de cada labio (hacia cada lado) [rad]
    std::uint32_t pending[2] = {~0u, ~0u}; // última membrana agregada en cada labio
    float sense = 0.0f; // sentido angular del lado "adelante" del anillo (el otro es opuesto); 0 = sin definir
    float coverage = 0.0f;        // ángulo alcanzado por los labios desde el contacto [rad]
    float bestCoverage = 0.0f;
    float stalled = 0.0f;         // tiempo sin que la copa avance [s]
    float preyInvMass = 1.0f;     // masa inversa original de la membrana de la presa
};

// Otra célula cerca de la presa: los labios no pueden meterse entre dos células pegadas.
struct CupObstacle {
    const RingBody* body;
    Vec2 shift; // lleva el cuerpo a la imagen más cercana a la presa
};

// La unión ligando–receptor sujeta a la presa al forro de la copa: mientras dura, la presa
// casi no cede en los contactos (si no, la copa la exprime hacia afuera como una semilla).
void holdPrey(const RingBody& prey, BodyParticles& bp, PhagocyticCup& cup);
void releasePrey(const RingBody& prey, BodyParticles& bp, const PhagocyticCup& cup);

// Primera partícula de membrana del depredador en contacto con la presa (o ~0u si no hay).
std::uint32_t findContact(const RingBody& predator, const BodyParticles& bp, const RingBody& prey,
                          Vec2 preyShift, const FeedingParams& params);

// Avanza el frente de cada labio y agrega membrana en el labio cuando el frente se aleja del
// forro (como las vesículas que se fusionan ahí). Modifica el anillo del depredador.
void growLips(RingBody& predator, BodyParticles& bp, const RingBody& prey, Vec2 preyShift,
              PhagocyticCup& cup, const FeedingParams& params, float dt,
              const std::vector<CupObstacle>& obstacles);

// Suma las velocidades de cremallera (forro del depredador) y adhesión (presa).
// `preyShift` lleva la presa a la imagen más cercana del mundo toroidal.
void applyCup(const RingBody& predator, BodyParticles& bp, const RingBody& prey, Vec2 preyShift,
              PhagocyticCup& cup, const FeedingParams& params, float dt,
              const std::vector<CupObstacle>& obstacles);

struct Closure {
    std::vector<std::uint32_t> outerRing; // anillo exterior que queda (incluye las puntas)
    std::vector<Vec2> pocket;             // polígono de la copa cerrada (coordenadas del depredador)
    Vec2 preyCenter;                      // centro de la presa en coordenadas del depredador
};

// ¿Se tocaron los labios con la presa adentro?
bool findClosure(const RingBody& predator, const BodyParticles& bp, Vec2 preyCenter,
                 const FeedingParams& params, Closure& out);
