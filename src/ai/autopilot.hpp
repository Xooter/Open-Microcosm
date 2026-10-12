#pragma once

#include "core/rng.hpp"
#include "core/vec2.hpp"

struct Locomotion;
struct RingBody;

// Componente: comportamiento mínimo de las células del linaje que el jugador no controla.
// Solo deambular para encontrar comida (la IA real —quimiotaxis, caza, huida— es la fase 7):
//  - ciliados: nado continuo con giros al azar (trayectorias que se curvan, como Paramecium);
//  - flagelados: nado hacia puntos que cambian cada pocos segundos;
//  - ameboides: pseudópodos hacia direcciones que cambian cada ~30 s.
struct Autopilot {
    float turn = 0.0f;
    float retarget = 0.0f; // tiempo hasta elegir un objetivo nuevo [s]
    Vec2 target;
};

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, Rng& rng, float dt);
