#pragma once

#include "bio/species.hpp"

#include <string>
#include <vector>

// Fisiología por especie con sus fuentes (MVP: validación). Vive en un archivo de datos
// (assets/species/fisiologia.toml, toml++): cada valor como se publicó —valor, unidad,
// temperatura de la medición y fuente— y el programa lo lleva a 20 °C con Q10 = 2 (la misma
// referencia que World::bioScale). Lo que no tiene medición directa para la especie se marca
// como estimación y se dice en la ficha.
struct SourcedValue {
    bool present = false;
    float value = 0.0f;       // en la unidad interna, a 20 °C (crecimiento: 1/h biológica)
    float published = 0.0f;   // como figura en la fuente
    std::string unit;         // la de la fuente: "1/h", "1/dia", "h" (duplicación), "dia" (duplicación)
    float temperature = 20.0f; // de la medición [°C]
    std::string source;
    bool estimate = false;
};

struct SpeciesPhysiology {
    SourcedValue growthMax; // tasa máxima de crecimiento de la biomasa [1/h biológica, 20 °C]
};

// La de la especie (cargada la primera vez desde assets/species/fisiologia.toml).
const SpeciesPhysiology& speciesPhysiology(SpeciesId id);
// Carga explícita (tests, otra ruta). Devuelve false y deja el motivo en `error` si falla; las
// especies sin datos quedan con `present = false` (el metabolismo usa el valor general).
bool loadSpeciesPhysiology(const std::string& path, std::string* error = nullptr);
// Avisos de la última carga (unidades desconocidas, especies inexistentes...).
const std::vector<std::string>& physiologyWarnings();
