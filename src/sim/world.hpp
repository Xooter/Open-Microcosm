#pragma once

#include "sim/particles.hpp"
#include "sim/rules.hpp"

#include <cstdint>
#include <vector>

// Mundo toroidal (los bordes se envuelven) con grilla espacial uniforme para
// que la búsqueda de vecinos sea O(n·k) en lugar de O(n²).
class World {
  public:
    World(float width, float height, std::size_t count, std::uint32_t seed);

    void step(float dt);
    void respawn(std::size_t count, std::uint32_t seed);

    const Particles& particles() const { return p_; }
    Rules& rules() { return rules_; }
    float width() const { return width_; }
    float height() const { return height_; }

  private:
    void rebuildGrid();
    void computeVelocities(float dt);
    void integrate(float dt);

    float width_, height_;
    Particles p_;
    Rules rules_;

    // Grilla: counting sort por celda. Copias ordenadas de x/y/species para que
    // los vecinos de una celda estén contiguos en memoria (amigable con la caché).
    int gridW_ = 0, gridH_ = 0;
    float cellW_ = 0, cellH_ = 0;
    std::vector<std::uint32_t> cellOf_;
    std::vector<std::uint32_t> cellStart_; // tamaño gridW*gridH + 1
    std::vector<float> sx_, sy_;
    std::vector<std::uint8_t> sSpecies_;
};
