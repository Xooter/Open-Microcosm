#pragma once

#include <algorithm>

// Afinamiento de los polos de una forma a lo largo de su eje (x local, el polo anterior en +x): el ancho
// se multiplica por una rampa lineal que vale 1 − `back` en el polo posterior (trompeta de Stentor, cola
// de Euglena) y 1 − `front` en el anterior (pera de Tetrahymena, probóscide de Dileptus). Con los dos,
// un huso (Phaeodactylum). 0 = sin afinar.
struct Taper {
    float back = 0.0f;
    float front = 0.0f;
};

// Factor del ancho en x, para una forma de semieje `rx`.
inline float taperFactor(const Taper& t, float x, float rx) {
    const float u = std::clamp(0.5f * (x / rx + 1.0f), 0.0f, 1.0f); // 0 en el polo posterior, 1 en el anterior
    return ((1.0f - t.back) + t.back * u) * ((1.0f - t.front) + t.front * (1.0f - u));
}
