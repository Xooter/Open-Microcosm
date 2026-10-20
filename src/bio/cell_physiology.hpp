#pragma once

#include "bio/genome.hpp"
#include "bio/metabolism.hpp"
#include "bio/species.hpp"
#include "core/rng.hpp"

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
    float light = 0.0f;       // irradiancia relativa [0, 1]
    float pigment = 0.0f;     // pigmento de estrés de la célula [0, 1] (filtra la luz)
    float signalToxin = 0.0f; // señal tóxica de otros (cuerpos R) [µM] (ver SignalToxicitySpec)
};

// µmax y medias saturaciones de la especie ([physiology.<id>.*], con su fuente) y los multiplicadores
// de su genoma. Sin herencia, los de la especie.
void applySpeciesPhysiology(MetabolicInput& in, SpeciesId id, const SpeciesInfo& info, const Heredity* heredity,
                            const MetabolismParams& params);

// Pigmento de estrés (astaxantina): sube con luz por encima del umbral de la especie o sin nitrógeno
// (más rápido en el quiste) y baja despacio sin estrés.
void updateStressPigment(const SpeciesInfo& info, float light, Limitation limiting, bool cyst, float bioDt,
                         Metabolism& m);

// Hierro que la lórica quiere precipitar del agua en `bioDt` [pmol]: se acerca al de una lórica completa
// (proporcional a la biomasa) en `loricaHours`.
float loricaIronDemand(const SpeciesInfo& info, const Metabolism& m, float bioDt);

// Fotosimbiontes (Paramecium bursaria, Mesodinium): actualiza su cantidad (división con luz o pérdida) y
// devuelve el azúcar que le pasan al huésped en `bioDt` [pmol de glucosa].
float stepPhotosymbionts(const SpeciesInfo& host, const MetabolismParams& p, Metabolism& m, float light, float bioDt);
// Al comer una presa: si es de la especie fuente, la retiene como simbionte (sus algas o sus
// cloroplastos). Devuelve true si la retuvo.
bool acquireSymbionts(const SpeciesInfo& host, SpeciesId prey, Metabolism& m);

// Metabolismo mínimo dentro de la pared del quiste.
void applyCyst(MetabolicInput& in);

// Estrés del ambiente (pH, temperatura, sal, O₂ en anaerobias, microcistina y violaceína ingerida):
// se paga como mantenimiento extra. `violacein`: la ingerida (se detoxifica de a poco; null si la
// célula no come). Devuelve la causa de muerte si el estrés es letal, o null.
const char* applyEnvironmentStress(const SpeciesInfo& info, const CellSurroundings& around, float* violacein,
                                   float bioDt, MetabolicInput& in);

// Biomasa al nacer que pide el genoma (tamaño `sizeScale` respecto del de la especie) [pmol]: se
// divide al duplicarla.
float targetBirthBiomass(SpeciesId species, float sizeScale);

// Las dos hijas de una división: cada una con su copia del genoma y sus mutaciones (`rate`: 0 en
// las células somáticas de un cultivo, que son clonales).
void daughterGenomes(const Heredity& mother, LocomotionType type, Rng& ra, Rng& rb, float rate, Heredity& a,
                     Heredity& b);

// Diatomeas: cada hija se queda con una valva de la madre y fabrica una nueva adentro, así que una
// de ellas es más chica (regla de MacDonald–Pfitzer). Por debajo de ~75 % del tamaño forma una
// auxospora y recupera el tamaño máximo. Se aplica a la hija chica.
void shrinkFrustule(Heredity& h, LocomotionType type);

// Lisis: la membrana revienta; ~40 % del contenido se disuelve (aminoácidos, azúcares) y el resto
// queda como detritos particulados.
inline constexpr float kLysisDissolved = 0.4f;
inline constexpr float kLysisParticulate = 0.6f;

// Osmotrofia (levaduras): lo que absorbe de la materia orgánica disuelta `organic` [µM] en `bioDt`
// [pmol] (Monod; transportadores de la pared menos afines que los de las bacterias).
float osmotrophDemand(float biomass, float organic, float bioDt);
