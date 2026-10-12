#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Partículas en layout SoA (Structure of Arrays): cada campo es un array contiguo.
// Esto es lo que permite vectorizar (SIMD) y paralelizar los loops calientes.
// Para agregar un rasgo biológico (energía, edad, genoma...) sumá otro vector y
// actualizalo en resize().
struct Particles {
    std::vector<float> x, y;
    std::vector<float> vx, vy;
    std::vector<std::uint8_t> species;

    std::size_t size() const { return x.size(); }

    void resize(std::size_t n) {
        x.resize(n);
        y.resize(n);
        vx.resize(n);
        vy.resize(n);
        species.resize(n);
    }
};
