#pragma once

#include "sim/dish.hpp"
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

    // Placa: las celdas de vidrio son fantasmas con el valor de su imagen especular en el agua,
    // así la pared no deja pasar nada (no-flujo) y las lecturas junto a ella no se diluyen. Lo
    // que un intercambio deje en una celda de vidrio vuelve a su celda de agua más cercana, y el
    // error de la imagen (la pared es curva) se corrige reescalando: se conserva la masa.
    void setMask(const DishMask* mask);
    // Suma de las celdas de agua (más lo pendiente de devolver desde el vidrio).
    double sum() const;

    float diffusion; // [µm²/s]

  private:
    Grid2D c_, tmp_;
    std::vector<Complex> spec_;
    const DishMask* mask_ = nullptr;
    std::vector<float> ghost_; // valor fantasma vigente de cada celda de vidrio

    void absorb();      // devuelve al agua lo que los intercambios dejaron en el vidrio
    void reflect();     // fantasmas = imagen especular
    double waterSum() const;
    void conserve(double before); // reescala el agua al total previo y vuelve a reflejar
};
