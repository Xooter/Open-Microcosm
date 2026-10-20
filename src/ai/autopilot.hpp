#pragma once

#include "core/rng.hpp"
#include "core/vec2.hpp"

#include <array>
#include <cstdint>

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
    float acid = 0.0f;  // log(1 + exceso de CO₂ sobre el equilibrio con el aire)
    // Solo si la especie tiene respuestas fóbicas o saltos (si no, quedan en 0):
    float light = 0.0f;  // irradiancia relativa en el centro
    float oxygen = 0.0f; // [µM]
    float shear = 0.0f;  // deformación del agua alrededor [1/s]
};

// Por qué la célula se mueve como se mueve en este momento (solo informativo: la ficha de
// inspección lo muestra; no cambia ninguna decisión).
enum class Intent : std::uint8_t {
    Wander,      // deambula (giros al azar, objetivos cambiantes)
    Chase,       // persigue una presa
    Flee,        // huye de un depredador
    AvoidThreat, // reacción de evitación ante una amenaza
    AvoidBump,   // reacción de evitación: el polo anterior chocó (mecanorrecepción)
    AcidTrap,    // trampa de ácido: vuelve a la zona con CO₂ (Jennings)
    Phototaxis,  // gira hacia el lado más iluminado
    Glide,       // diatomea: se desliza por el rafe, con inversiones
    Walk,        // hipotrico: camina sobre los cirros
    Pause,       // parada de un caminante
    Jump,        // salto (cirros o cerdas saltadoras)
    Phobic,      // respuesta fóbica a un cambio brusco (luz, O₂, corriente)
    Contract,    // se contrajo entera (mionemas) y se está re-extendiendo
};

// Componente: comportamiento de las células que no controla el jugador. Sale de los datos de
// la especie (SpeciesInfo), no de código por especie:
//  - ciliados: nado con giros al azar; quimiocinesis (si la comida mejora, giran menos);
//    reacción de evitación (retroceder y girar) ante una amenaza; los depredadores persiguen;
//  - flagelados: nado hacia objetivos que cambian; fototaxis; huida;
//  - ameboides: pseudópodo hacia la presa más cercana o hacia direcciones al azar.
struct Autopilot {
    Intent intent = Intent::Wander;
    float turn = 0.0f;
    float retarget = 0.0f; // tiempo hasta elegir un objetivo nuevo [s]
    Vec2 target;
    float memory = 0.0f;   // señal de comida promediada (quimiocinesis temporal)
    float reverse = 0.0f;  // tiempo restante de la reacción de evitación [s]
    bool walking = true;   // hipotricos: alternan caminatas y paradas
    float acidMemory = 0.0f;
    // Respuestas fóbicas (SpeciesInfo::responses): nivel recordado de cada una (< 0: sin medir todavía).
    std::array<float, 4> responseMemory{-1.0f, -1.0f, -1.0f, -1.0f};
    float refractory = 0.0f; // [s] sin volver a responder
    float jump = 0.0f;       // [s] que le quedan al salto en curso
    float nextJump = -1.0f;  // [s] hasta el próximo salto espontáneo (< 0: sin sortear)
    bool contract = false;   // en este paso disparó la contracción del cuerpo (la aplica el mundo)
};

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, const SpeciesInfo& species,
                    const Perception& sense, Rng& rng, float dt);
