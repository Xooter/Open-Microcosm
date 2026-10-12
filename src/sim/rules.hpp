#pragma once

#include <cstdint>

inline constexpr int kMaxSpecies = 8;

// Reglas "biológicas" de interacción entre especies (modelo tipo Particle Life).
// attraction[a][b] > 0: a es atraída por b; < 0: a huye de b. No es simétrica,
// lo que genera comportamientos emergentes (persecución, células, membranas...).
struct Rules {
    int speciesCount = 6;
    float attraction[kMaxSpecies][kMaxSpecies]{};

    float rMax = 40.0f;            // radio de percepción (px de mundo)
    float beta = 0.3f;             // zona de repulsión corta (fracción de rMax), evita colapsos
    float forceScale = 10.0f;
    float frictionHalfLife = 0.04f; // segundos para perder la mitad de la velocidad

    void randomize(std::uint32_t seed);
};

// Kernel de fuerza en función de la distancia normalizada r = d / rMax en [0, 1).
inline float interactionKernel(float r, float a, float beta) {
    if (r < beta)
        return r / beta - 1.0f;
    if (r < 1.0f) {
        float t = 2.0f * r - 1.0f - beta;
        return a * (1.0f - (t < 0 ? -t : t) / (1.0f - beta));
    }
    return 0.0f;
}
