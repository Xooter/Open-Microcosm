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
    // Atenuación por celda (0 en el vidrio de la placa, 1 en el agua libre). nullptr = sin pared.
    void setFlowMask(const std::vector<float>* mask) { flowMask_ = mask; }

    Vec2 velocityAt(Vec2 p) const { return {ux_.sample(p.x, p.y), uy_.sample(p.x, p.y)}; }
    // Guarda la solución actual como "fondo": la que siente un nadador sin su propio flujo
    // (a bajo Reynolds, su velocidad de nado ya incluye el efecto de su propio flujo).
    void keepAsBackground() {
        bgx_.v = ux_.v;
        bgy_.v = uy_.v;
    }
    Vec2 backgroundAt(Vec2 p) const { return {bgx_.sample(p.x, p.y), bgy_.sample(p.x, p.y)}; }
    const Grid2D& ux() const { return ux_; }
    const Grid2D& uy() const { return uy_; }

    float brinkmanFriction() const { return 12.0f * viscosity / (gapHeight * gapHeight); }

    float viscosity = units::kWaterKinematicViscosity; // [µm²/s]
    float gapHeight = 200.0f;                          // espesor de la película [µm]

  private:
    Grid2D fx_, fy_, ux_, uy_, bgx_, bgy_;
    std::vector<Complex> bx_, by_;
    const std::vector<float>* flowMask_ = nullptr;
};
