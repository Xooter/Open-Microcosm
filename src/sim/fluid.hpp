#pragma once

#include "core/units.hpp"
#include "core/vec2.hpp"
#include "sim/fft.hpp"
#include "sim/grid2d.hpp"

// Agua a bajo número de Reynolds (Re ~ 1e-4 a escala de protistas): no hay inercia, la
// velocidad es la respuesta instantánea a las fuerzas (ecuaciones de Stokes). La película de
// agua entre portaobjetos y cubreobjetos agrega fricción de Hele-Shaw (término de Brinkman
// 12ν/h²), que hace que las corrientes sean locales: se apagan a ~h/√12 de la fuerza.
//
// Se resuelve exacto en Fourier: û = P·f̂ / (ν k² + α), con P la proyección incompresible.
class Fluid {
  public:
    Fluid(int nx, int ny, float cellSize);

    void clearForces();
    // Fuerza por unidad de masa [µm/s²] con perfil gaussiano de radio `radius` [µm].
    void addForce(Vec2 center, Vec2 force, float radius);
    void solve(const Fft2D& fft);

    Vec2 velocityAt(Vec2 p) const { return {ux_.sample(p.x, p.y), uy_.sample(p.x, p.y)}; }
    const Grid2D& ux() const { return ux_; }
    const Grid2D& uy() const { return uy_; }

    float brinkmanFriction() const { return 12.0f * viscosity / (gapHeight * gapHeight); }

    float viscosity = units::kWaterKinematicViscosity; // [µm²/s]
    float gapHeight = 200.0f;                          // espesor de la película [µm]

  private:
    Grid2D fx_, fy_, ux_, uy_;
    std::vector<Complex> bx_, by_;
};
