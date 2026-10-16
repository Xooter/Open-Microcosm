#pragma once

#include "bio/genome.hpp"
#include "bio/metabolism.hpp"
#include "bio/species.hpp"

// Fisiología de una célula, común a la placa (`World`) y al cultivo bien mezclado (fase P1). Entre
// los dos cambia solo de dónde sale el entorno (el campo junto a la célula o la media del medio) y
// adónde va lo que la célula toma y libera: así las dos escalas corren la misma biología.

// Lo que la célula tiene alrededor.
struct CellSurroundings {
    float o2 = 0.0f;          // [µM]
    float ph = 7.0f;
    float microcystin = 0.0f; // [µM]
    float temperature = 20.0f; // [°C]
    float salinity = 0.0f;    // [PSU]
};

// µmax y medias saturaciones de la especie (fisiologia.toml, con su fuente) y los multiplicadores
// de su genoma. Sin herencia, los de la especie.
void applySpeciesPhysiology(MetabolicInput& in, SpeciesId id, const SpeciesInfo& info, const Heredity* heredity,
                            const MetabolismParams& params);

// Metabolismo mínimo dentro de la pared del quiste.
void applyCyst(MetabolicInput& in);

// Estrés del ambiente (pH, temperatura, sal, O₂ en anaerobias, microcistina y violaceína ingerida):
// se paga como mantenimiento extra. `violacein`: la ingerida (se detoxifica de a poco; null si la
// célula no come). Devuelve la causa de muerte si el estrés es letal, o null.
const char* applyEnvironmentStress(const SpeciesInfo& info, const CellSurroundings& around, float* violacein,
                                   float bioDt, MetabolicInput& in);

// Osmotrofia (levaduras): lo que absorbe de la materia orgánica disuelta `organic` [µM] en `bioDt`
// [pmol] (Monod; transportadores de la pared menos afines que los de las bacterias).
float osmotrophDemand(float biomass, float organic, float bioDt);
