#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>
#include <vector>

struct RingBody;
class Fluid;

enum class LocomotionType : std::uint8_t {
    Cilia,     // ciliados (Paramecium): deslizamiento continuo, giro por batido diferencial
    Flagellum, // flagelados (Euglena, Chlamydomonas): un flagelo anterior que tira
    Pseudopod, // ameboides (Amoeba): protrusión del frente y contracción de la cola
    // Células animales adheridas al fondo (fibroblastos, neutrófilos): el lamelipodio protruye
    // en el frente y las adhesiones focales tiran del cuerpo. Es biología (polimerización de
    // actina), así que corre en tiempo biológico: unos µm/min reales son µm/s con kBioTimeScale 60.
    Crawl,
    // Sin locomoción propia: células de un tejido (con pared), escamosas descamadas, leucocitos de
    // un frotis. No tienen cilios ni flagelo (los genes de tricocistos y cloroplastos no aplican).
    None,
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

// Velocidad máxima de nado o de avance de cada tipo de locomoción [µm/s].
float maxSpeedOf(const LocomotionParams& p, LocomotionType type);

// Componente: control y estado del aparato locomotor.
struct Locomotion {
    LocomotionType type = LocomotionType::Cilia;
    float forward = 0.0f; // [-1, 1] (cilios)
    float turn = 0.0f;    // [-1, 1] (cilios)
    bool active = false;  // flagelo batiendo / pseudópodo extendiéndose
    Vec2 target;          // hacia dónde gira el flagelado o se estira el pseudópodo
    float speed = 0.0f;   // velocidad de nado actual [µm/s] (para el agua y el render)
    float power = 1.0f;   // multiplicador genético de la velocidad
    float helixRadius = 0.0f; // nado helicoidal (de la especie) [µm]; 0 = bamboleo genérico
    float helixPeriod = 0.0f; // [s]
    float phase = 0.0f;       // fase propia de la hélice [rad] (cada célula gira a su tiempo)
    bool gliding = false;     // se desliza sobre el vidrio con el flagelo (no nada)
    float burst = 0.0f;       // salto en curso: velocidad a lo largo del eje [µm/s] (0: no salta)
    bool pusher = false;      // flagelo posterior: empuja desde atrás
    bool aboralFirst = false; // cilios: avanza con el polo posterior adelante (telotroco de los peritricos)
    bool testate = false;     // tecameba: los seudópodos salen por la abertura y arrastran la testa rígida
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
// `oralCurrent`: la célula se alimenta filtrando (cilios del surco oral o del peristoma).
void applySwimmerForces(const RingBody& body, const BodyParticles& bp, const Locomotion& loco,
                        const LocomotionParams& params, Fluid& fluid, bool oralCurrent);
