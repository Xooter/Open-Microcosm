#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"
#include "sim/contacts.hpp"
#include "sim/dish.hpp"
#include "sim/soft_body.hpp"

#include <cstdint>
#include <vector>

// Unión de adhesión entre membranas de cuerpos distintos (filamentos, colonias, metazoos).
struct LinkConstraint {
    std::uint32_t a, b; // slots de membrana
    float rest;         // [µm]
};

// Todo lo que necesita el bucle de sub-pasos de XPBD de un paso (fase 14). El mundo lo arma en
// CPU; un resolvedor alternativo (GPU) puede ejecutarlo entero y devolver las posiciones.
struct SubstepInput {
    const std::vector<RingBody*>* bodies = nullptr;
    const std::vector<std::uint8_t>* lod = nullptr;     // 2 detalle, 1 simplificado, 0 estadístico
    const std::vector<std::uint8_t>* touch = nullptr;   // cuerpos que pueden chocar en este paso
    const std::vector<ContactMember>* members = nullptr;
    const std::vector<std::uint32_t>* memberBody = nullptr;
    const std::vector<LinkConstraint>* links = nullptr;
    const SoftBodyTuning* tuning = nullptr;
    float linkRate = 300.0f;            // [1/s]
    std::uint32_t grabSlot = ~0u;       // punto agarrado con el mouse (~0u: ninguno)
    Vec2 grabTarget;
    float dt = 0.0f, width = 0.0f, height = 0.0f, maxRadius = 0.0f;
    int stride = 1;                     // LOD: los simplificados se resuelven 1 de cada `stride`
    Dish dish;                          // pared de la placa (inactiva: mundo toroidal)
};

// Resolvedor alternativo del bucle de sub-pasos (predicción, restricciones de cada cuerpo,
// contactos y uniones). `sim` no conoce a la GPU: la implementación vive en el ejecutable.
class SubstepSolver {
  public:
    virtual ~SubstepSolver() = default;
    virtual const char* name() const = 0;
    // Ejecuta todos los sub-pasos y deja las posiciones nuevas en `bp`. Si devuelve false (no
    // pudo), el mundo usa la CPU.
    virtual bool run(BodyParticles& bp, const SubstepInput& in) { return submit(bp, in) && collect(bp); }
    // En dos tiempos, para que la CPU haga otra cosa mientras tanto: `submit` lanza el trabajo
    // (no toca `bp`) y `collect` espera y copia las posiciones (false: descartar y usar la CPU).
    virtual bool submit(const BodyParticles& bp, const SubstepInput& in) = 0;
    virtual bool collect(BodyParticles& bp) = 0;
};
