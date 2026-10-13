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
    Undulation, // nematodos: ondas de flexión del cuerpo entero (músculos longitudinales)
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
    // Nematodo (Rhabditis, C. elegans): en agua libre se agita a ~2 Hz con ondas largas y avanza
    // poco por ciclo; entre partículas (o sobre agar) repta a ~0,5–1 Hz con ondas cortas y avanza
    // mucho más (Fang-Yen et al. 2010). Largos de onda en largos del cuerpo; amplitud: ángulo
    // máximo del eje [rad].
    float swimHz = 2.0f, crawlHz = 0.8f;
    float swimWavelength = 1.5f, crawlWavelength = 0.7f;
    float swimAmplitude = 1.1f, crawlAmplitude = 0.8f; // nadando forma una "C"; reptando, una S más chata
    // Arrastre normal / tangencial de un cuerpo delgado: ~1,5 en agua; en un medio granular o
    // en un gel el arrastre lateral es mucho mayor (el cuerpo "se apoya" de costado).
    float dragRatioWater = 1.5f, dragRatioSubstrate = 8.0f;
    float undulationTurn = 1.2f;   // flexión de giro sumada a la onda [rad de punta a punta]
    float undulationSpeed = 300.0f; // referencia para el costo de nadar [µm/s]
};

// Componente: control y estado del aparato locomotor.
struct Locomotion {
    LocomotionType type = LocomotionType::Cilia;
    float forward = 0.0f; // [-1, 1] (cilios)
    float turn = 0.0f;    // [-1, 1] (cilios)
    bool active = false;  // flagelo batiendo / pseudópodo extendiéndose
    Vec2 target;          // hacia dónde gira el flagelado o se estira el pseudópodo
    float speed = 0.0f;   // velocidad de nado actual [µm/s] (para el agua y el render)
    float power = 1.0f;   // multiplicador genético de la velocidad
    std::uint32_t mouthRing = 0; // índice del anillo con el citostoma (boca)
};

// Componente: cuerpo de nematodo. La forma de reposo de la membrana sigue una onda de
// curvatura (los músculos); el avance sale de la teoría de fuerzas resistivas: a bajo número de
// Reynolds, la traslación y la rotación son las que dejan fuerza y torque netos nulos.
struct Undulation {
    // Coordenadas de la forma recta: posición a lo largo del eje y desvío de costado [µm].
    std::vector<float> ringS, ringOffset, interiorS, interiorOffset;
    float length = 0.0f;    // largo del eje [µm]
    float phase = 0.0f;     // fase de la onda [rad]
    float crowding = 0.0f;  // 0: agua libre; 1: entre partículas y granos
    float effort = 0.0f;    // 0: relajado (cuerpo recto); 1: ondulando a pleno (los músculos se activan)
    float dragRatio = 1.5f; // ξn/ξt del último paso
    std::vector<Vec2> spine;                 // eje en el mundo (render)
    std::vector<Vec2> waterAt, waterPush;    // fuerzas del cuerpo sobre el agua (paso anterior)
};

// Componente marcador de la célula controlada por el jugador.
struct Player {};

Vec2 bodyHeading(const RingBody& body);

// Movimiento rígido en el plano que impuso la propulsión (para arrastrar las partes de un
// cuerpo compuesto: la corona del rotífero mueve a todo el animal).
struct RigidMotion {
    Vec2 linear;
    float omega = 0.0f;
    Vec2 center;
};

// Suma la propulsión a la velocidad externa del paso (bp.vx/vy) de cada partícula del cuerpo.
RigidMotion applyLocomotion(const RingBody& body, BodyParticles& bp, Locomotion& loco,
                     const LocomotionParams& params, float simTime, float width, float height);

// Nematodo: prepara la forma recta (llamar al crearlo).
void initUndulation(const RingBody& body, Undulation& und);
// Avanza la onda muscular (forward > 0: de la cabeza a la cola, avanza; < 0: retrocede), mueve
// la forma de reposo y suma a las partículas el movimiento rígido que dejan las fuerzas
// resistivas. `crowding` (0–1): cuánto sólido hay alrededor (partículas asentadas, granos).
RigidMotion undulate(RingBody& body, BodyParticles& bp, Locomotion& loco, Undulation& und, const LocomotionParams& p,
                     float crowding, float dt);
// El cuerpo empuja el agua con las fuerzas de arrastre del paso anterior (suman cero).
void applyUndulationForces(const Undulation& und, Fluid& fluid);

// Fuerzas que el nadador ejerce sobre el agua. Un microorganismo nada sin fuerza neta
// (empuje = arrastre), así que en el agua deja un par de fuerzas opuestas.
// `oralCurrent`: la célula se alimenta filtrando (cilios del surco oral o del peristoma).
void applySwimmerForces(const RingBody& body, const BodyParticles& bp, const Locomotion& loco,
                        const LocomotionParams& params, Fluid& fluid, bool oralCurrent);
