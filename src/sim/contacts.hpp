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
};

// Colisiones entre membranas (de cuerpos distintos y de un cuerpo consigo mismo, para que no
// se pliegue). Cada punto se compara contra ARISTAS vecinas, no solo contra puntos: así no se
// cuela por los huecos que se abren cuando la membrana se estira.
// Esquema Jacobi: cada partícula calcula y escribe SOLO su propia corrección, así que es
// paralelo y determinista.
class ContactSolver {
  public:
    // Construye la grilla de vecinos (una vez por sub-paso).
    void prepare(const BodyParticles& bp, const std::vector<ContactMember>& members, float width,
                 float height, float maxRadius);
    // Una pasada Jacobi. Se intercala con las restricciones de cada cuerpo.
    void iterate(BodyParticles& bp, const std::vector<ContactMember>& members, float width,
                 float height);

  private:
    SpatialGrid grid_;
    std::vector<float> wx_, wy_, dx_, dy_;
};
