#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class ParticleKind : std::uint8_t {
    Ink,      // tinta china: partículas de carbono de ~0.2 µm, trazador del flujo
    Detritus, // materia orgánica muerta: se descompone liberando nutrientes disueltos
};

// Partículas ambientales en layout SoA (un array contiguo por campo).
// Para agregar un campo nuevo, sumalo también en push() y swapRemove().
struct AmbientParticles {
    std::vector<float> x, y;
    std::vector<float> radius; // [µm]
    std::vector<float> mass;   // [pmol equivalentes de glucosa], 0 para la tinta
    std::vector<float> age;    // [s]
    std::vector<ParticleKind> kind;

    std::size_t size() const { return x.size(); }

    void push(ParticleKind k, float px, float py, float r, float m) {
        x.push_back(px);
        y.push_back(py);
        radius.push_back(r);
        mass.push_back(m);
        age.push_back(0.0f);
        kind.push_back(k);
    }

    // Swap-and-pop: O(1), pero mueve la última partícula al índice i.
    void swapRemove(std::size_t i) {
        const std::size_t last = size() - 1;
        x[i] = x[last];
        y[i] = y[last];
        radius[i] = radius[last];
        mass[i] = mass[last];
        age[i] = age[last];
        kind[i] = kind[last];
        x.pop_back();
        y.pop_back();
        radius.pop_back();
        mass.pop_back();
        age.pop_back();
        kind.pop_back();
    }
};
