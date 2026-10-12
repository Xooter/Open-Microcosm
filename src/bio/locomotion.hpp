#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>

struct RingBody;
class Fluid;

enum class LocomotionType : std::uint8_t {
    Cilia,     // ciliados (Paramecium): deslizamiento continuo, giro por batido diferencial
    Flagellum, // flagelados (Euglena, Chlamydomonas): un flagelo anterior que tira
    Pseudopod, // ameboides (Amoeba): protrusión del frente y contracción de la cola
};

const char* locomotionName(LocomotionType type);

// Velocidades de laboratorio. A bajo Reynolds no hay inercia: la célula se mueve solo
// mientras propulsa, y frena en el acto.
struct LocomotionParams {
    float ciliaSpeed = 600.0f;     // Paramecium caudatum: ~0.6–1 mm/s [µm/s]
    float ciliaReverse = 0.5f;     // marcha atrás (reacción de evitación), fracción
    float ciliaTurnRate = 2.5f;    // [rad/s]
    float flagellumSpeed = 80.0f;  // Euglena gracilis: ~50–100 µm/s
    float flagellumTurnRate = 2.0f;
    float wobbleHz = 1.5f;         // rotación helicoidal de Euglena (~1–2 Hz)
    float wobble = 0.15f;          // amplitud lateral relativa
    float pseudopodSpeed = 6.0f;   // protrusión de ameboides: ~1–10 µm/s
    float retractSpeed = 3.0f;     // contracción de la cola (actomiosina)
    float streamingSpeed = 6.0f;   // corriente citoplasmática hacia el pseudópodo
    float oralCurrent = 80.0f;     // corriente de alimentación del ciliado hacia la boca
};

// Componente: control y estado del aparato locomotor.
struct Locomotion {
    LocomotionType type = LocomotionType::Cilia;
    float forward = 0.0f; // [-1, 1] (cilios)
    float turn = 0.0f;    // [-1, 1] (cilios)
    bool active = false;  // flagelo batiendo / pseudópodo extendiéndose
    Vec2 target;          // hacia dónde gira el flagelado o se estira el pseudópodo
    float speed = 0.0f;   // velocidad de nado actual [µm/s] (para el agua y el render)
    std::uint32_t mouthRing = 0; // índice del anillo con el citostoma (boca)
};

// Componente marcador de la célula controlada por el jugador.
struct Player {};

Vec2 bodyHeading(const RingBody& body);

// Suma la propulsión a la velocidad externa del paso (bp.vx/vy) de cada partícula del cuerpo.
void applyLocomotion(const RingBody& body, BodyParticles& bp, Locomotion& loco,
                     const LocomotionParams& params, float simTime, float width, float height);

// Fuerzas que el nadador ejerce sobre el agua. Un microorganismo nada sin fuerza neta
// (empuje = arrastre), así que en el agua deja un par de fuerzas opuestas.
void applySwimmerForces(const RingBody& body, const BodyParticles& bp, const Locomotion& loco,
                        const LocomotionParams& params, Fluid& fluid);
