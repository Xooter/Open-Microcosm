#pragma once

#include "sim/body_particles.hpp"

#include "core/vec2.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct RingBody;

// Componente: proporciones y rigidez propias de la especie (se usan para rehacer la forma de
// reposo de las hijas después de una división).
struct CellShape {
    float radiusX, radiusY; // semiejes al nacer [µm]
    float shapeRate;        // memoria de forma propia [1/s]
    float nucleusFraction;
    float squareness = 2.0f; // superelipse (2 = elipse)
    float taper = 0.0f;
};

// Componente: división en curso (fisión binaria).
//  1. Se duplican el núcleo y la vacuola contráctil, que se separan hacia los dos polos.
//  2. El anillo contráctil (actomiosina) estrangula la célula perpendicular a su eje largo
//     (fisión transversal, como en Paramecium).
//  3. Cuando los dos lados del surco se tocan, la membrana se parte en dos anillos.
struct Dividing {
    float progress = 0.0f;        // [0, 1] en tiempo biológico (puede pasar de 1 si se demora)
    std::uint32_t nucleus2 = ~0u; // núcleo hijo
    std::uint32_t vacuole2 = ~0u; // vacuola contráctil hija
    // Forma de reposo de la madre al empezar: se transforma de a poco en dos hijas.
    std::vector<Vec2> motherRest;
    float motherArea = 0.0f;
};

// Forma de reposo en división: interpola cada punto entre la elipse madre y su lugar en dos
// elipses hijas (mitad de área, misma forma) que se tocan en el plano de división.
// `t` en [0, 1]. Las hijas tienen √2 veces el perímetro de la madre: la membrana nueva sale
// de la propia forma (en la célula real, de vesículas que se fusionan en el surco).
void divisionRestShape(const std::vector<Vec2>& mother, float scale, float t, std::vector<Vec2>& out);

// Lo mismo para un contorno cualquiera (silueta con cuernos o brazos): la mitad anterior de la madre
// (x ≥ 0 en su marco) va a la silueta entera de una hija, con su polo posterior en el plano de
// división, y la posterior a la otra hija, con su polo anterior ahí; las dos con el anterior hacia +x,
// como en la fisión transversal. `outline` es la silueta de la especie (tamaño típico); cada hija
// tiene la mitad del área de la madre. Devuelve false si la mitad anterior no es un solo tramo del
// anillo (el que llama usa entonces la versión elíptica).
bool divisionRestShapeOutline(const std::vector<Vec2>& mother, const std::vector<Vec2>& outline, float outlineArea,
                              float scale, float t, std::vector<Vec2>& out);

// `bodyHalfLength` [µm]: el semieje del cuerpo, cuando el anillo tiene cuernos o brazos (la franja del
// surco se mide desde el cuerpo, no desde la punta de un cuerno). 0: la mitad del largo del anillo.

// Velocidades del surco: la membrana cercana al plano de división se cierra hacia el eje.
void applyFurrow(const RingBody& body, BodyParticles& bp, float speed, float bodyHalfLength = 0.0f);

// Media anchura actual del surco (distancia de su fondo al eje) [µm].
float furrowHalfWidth(const RingBody& body, const BodyParticles& bp, float bodyHalfLength = 0.0f);

// ¿Se tocaron los dos lados del surco? Devuelve los índices de anillo de sus fondos.
bool furrowClosed(const RingBody& body, const BodyParticles& bp, std::size_t& tipA, std::size_t& tipB,
                  float bodyHalfLength = 0.0f);
