#pragma once

#include "sim/particles.hpp"

#include <raylib.h>

#include <cstdint>

// Sedimento inorgánico en campo claro. Los minerales transparentes (cuarzo, mica, sílice) se
// ven por su relieve: borde oscuro grueso y, adentro, la línea de Becke brillante (el índice
// del cuarzo, 1,54, es mayor que el del agua, 1,33). Los opacos (óxidos) se ven pardos y llenos.

// Partícula suelta de radio `radius` (en las unidades del dibujo) orientada `angle` [rad].
// `minSize`: tamaño mínimo visible.
void drawMineral(MineralKind kind, Vector2 c, float radius, float angle, float minSize);

// Grano de arena: contorno `pts` (n puntos en el orden del anillo, alrededor de `c`).
// `edge`: grosor del borde en las unidades del dibujo; `seed`: fracturas e inclusiones.
void drawRock(const Vector2* pts, int n, Vector2 c, MineralKind kind, std::uint32_t seed, float edge);
