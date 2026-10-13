#pragma once

#include "sim/body_particles.hpp"
#include "sim/spatial_grid.hpp"

#include <cstdint>
#include <vector>

struct ContactMember {
    std::uint32_t slot;
    std::uint32_t next; // índice (en members) del siguiente punto del anillo: forma la arista
    std::uint32_t owner;
    std::uint32_t ringPos, ringSize; // para ignorar vecinos cercanos de la misma membrana
    std::uint32_t group = 0; // partes de un mismo organismo compuesto no chocan entre sí
    std::uint8_t selfContact = 0; // membrana plástica: puede chocar consigo misma (pliegues)
};

// Colisiones entre membranas (de cuerpos distintos y de un cuerpo consigo mismo, para que no
// se pliegue). Cada punto se compara contra ARISTAS vecinas, no solo contra puntos: así no se
// cuela por los huecos que se abren cuando la membrana se estira.
// Esquema Jacobi: cada partícula calcula y escribe SOLO su propia corrección, así que es
// paralelo y determinista.
class ContactSolver {
  public:
    // Construye la grilla de vecinos (una vez por sub-paso).
    // `active` (opcional): los puntos que se van a corregir en este sub-paso.
    void prepare(const BodyParticles& bp, const std::vector<ContactMember>& members, float width,
                 float height, float maxRadius, const std::vector<std::uint8_t>* active = nullptr);
    // Una pasada Jacobi. Se intercala con las restricciones de cada cuerpo.
    // `active` (opcional): solo esos puntos se corrigen en esta pasada (LOD); los demás siguen
    // contando como obstáculo.
    void iterate(BodyParticles& bp, const std::vector<ContactMember>& members, float width,
                 float height, const std::vector<std::uint8_t>* active = nullptr);

  private:
    SpatialGrid grid_;
    std::vector<float> wx_, wy_, dx_, dy_;
    // Puntos con algún vecino de otro cuerpo (o de sí mismo, si es plástico) en su vecindario:
    // los demás no pueden chocar en este sub-paso y se saltean.
    std::vector<std::uint8_t> crowded_;
};
