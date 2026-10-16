#pragma once

#include "bio/species.hpp"

#include <cstddef>
#include <cstdint>

// Calibración cruzada placa ↔ cultivo (fase P1, paso 2). La placa simula la alimentación con
// física (la boca, la corriente oral, el nado, la altura en la película) y el cultivo con la
// respuesta funcional de los datos; las dos tienen que dar lo mismo a la misma densidad de presas.
// Si no, una de las dos está mal: se decide con los datos publicados, no ajustando.
struct FunctionalResponsePoint {
    double density = 0.0; // bacterias por µm³ (media en el tiempo medido)
    double plate = 0.0;   // bacterias por segundo biológico y por individuo, en la placa
    double culture = 0.0; // lo mismo con la respuesta funcional del cultivo
    int individuals = 0;
};

// Mide en una placa de ~0,86 mm, a 20 °C y sin otros seres, cuántas bacterias come cada uno de
// `individuals` filtradores de la especie en `seconds` segundos, con `bacteria` bacterias.
FunctionalResponsePoint measureFunctionalResponse(SpeciesId id, std::size_t bacteria, int individuals, float seconds,
                                                  std::uint64_t seed = 2026);
