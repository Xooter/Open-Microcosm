#pragma once

#include "core/vec2.hpp"

#include <cstdint>

// Células de organismos pluricelulares en cultivo (fase 20b): fibroblastos, HeLa, neutrófilos.
// Lo que las distingue de un protista es que viven pegadas al fondo, migran por arrastre, se
// dividen por un ciclo con fases y controles (punto de restricción, inhibición por contacto) y
// mueren por apoptosis (un suicidio ordenado, sin reventar) cuando les falta lo que necesitan.

// Fases del ciclo celular. G0: quiescente (fuera del ciclo: sin factores de crecimiento o con la
// periferia ocupada por vecinas). Apoptotic: ejecutando la apoptosis. Dead: muerta sin desarmarse
// (`AnimalCellInfo::immotileDeath`: el espermatozoide queda entero e inmóvil, sin metabolismo).
enum class CellPhase : std::uint8_t { G0, G1, S, G2, M, Apoptotic, Dead };
const char* cellPhaseName(CellPhase phase);

// Clase de motilidad de una célula que nada (OMS 2021). Muerta no es una clase: es CellPhase::Dead.
enum class MotilityGrade : std::uint8_t { Progressive, NonProgressive, Immotile };
const char* motilityGradeName(MotilityGrade grade);

// Componente: estado de una célula animal.
struct AnimalCell {
    CellPhase phase = CellPhase::G1;
    float phaseHours = 0.0f;     // tiempo biológico en la fase actual [h]
    float polarity = 0.0f;       // dirección del frente de migración [rad]
    float contact = 0.0f;        // fracción del contorno que toca a otra célula (0–1)
    Vec2 contactDirection;       // hacia dónde están las vecinas que toca (unitario o cero)
    float starvedHours = 0.0f;   // horas sin energía (sin glucosa ni reservas)
    float serumlessHours = 0.0f; // horas sin factores de crecimiento
    float ageHours = 0.0f;       // edad (el neutrófilo vive horas)
    float apoptosis = 0.0f;      // avance de la apoptosis (0–1)
    bool repolarized = false;    // ya se dio vuelta por este contacto (Abercrombie)
    // Extensión sobre el fondo (0: redonda, recién sembrada; 1: extendida). Una célula que cae de
    // la suspensión se adhiere y se extiende en ~1 h.
    float spread = 1.0f;
    // Las que nadan ([being.motility]): su clase y su rapidez respecto de la media de la especie.
    MotilityGrade grade = MotilityGrade::Progressive;
    float vigor = 1.0f;
    float swelling = 0.0f; // cola hinchada y enrollada por ósmosis (prueba hiposmótica), 0–1
};

// Componente marcador: adherida al sustrato por adhesiones focales. El agua no la arrastra (una
// corriente de una gota no alcanza a despegar una célula adherida; sí la que está en mitosis o en
// apoptosis, que se redondea y se suelta).
struct Adherent {};
