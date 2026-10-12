#pragma once

#include "core/rng.hpp"
#include "core/vec2.hpp"

struct Locomotion;
struct RingBody;
struct SpeciesInfo;

// Lo que una célula percibe en este paso (lo calcula el mundo).
struct Perception {
    bool hasPrey = false;
    Vec2 prey;          // centro de la presa más cercana (en la imagen cercana)
    float preyDistance = 0.0f;
    bool hasThreat = false;
    Vec2 threat;
    float threatDistance = 0.0f;
    float food = 0.0f;  // log(1 + nutrientes) en la célula
    bool bumped = false; // el polo anterior tocó otra célula (mecanorrecepción)
    float lightLeft = 0.0f, lightRight = 0.0f; // a cada lado del polo anterior
};

// Componente: comportamiento de las células que no controla el jugador. Sale de los datos de
// la especie (SpeciesInfo), no de código por especie:
//  - ciliados: nado con giros al azar; quimiocinesis (si la comida mejora, giran menos);
//    reacción de evitación (retroceder y girar) ante una amenaza; los depredadores persiguen;
//  - flagelados: nado hacia objetivos que cambian; fototaxis; huida;
//  - ameboides: pseudópodo hacia la presa más cercana o hacia direcciones al azar.
struct Autopilot {
    float turn = 0.0f;
    float retarget = 0.0f; // tiempo hasta elegir un objetivo nuevo [s]
    Vec2 target;
    float memory = 0.0f;   // señal de comida promediada (quimiocinesis temporal)
    float reverse = 0.0f;  // tiempo restante de la reacción de evitación [s]
};

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, const SpeciesInfo& species,
                    const Perception& sense, Rng& rng, float dt);
