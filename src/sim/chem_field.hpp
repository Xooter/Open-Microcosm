#pragma once

#include "sim/fft.hpp"
#include "sim/grid2d.hpp"

class Fluid;

// Concentración química disuelta [µM] que se difunde y es arrastrada por el agua.
class ChemField {
  public:
    ChemField(int nx, int ny, float cellSize, float diffusion, float initial);

    Grid2D& grid() { return c_; }
    const Grid2D& grid() const { return c_; }

    // Advección semi-lagrangiana: estable, pero no conserva masa exactamente.
    void advect(const Fluid& fluid, float dt);
    // Difusión exacta en Fourier: ĉ·exp(-D k² dt). Estable con cualquier dt y conserva masa.
    void diffuse(const Fft2D& fft, float dt);

    float diffusion; // [µm²/s]

  private:
    Grid2D c_, tmp_;
    std::vector<Complex> spec_;
};
