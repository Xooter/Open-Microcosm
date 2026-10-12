#pragma once

#include "bio/locomotion.hpp"
#include "core/rng.hpp"

#include <array>
#include <cstddef>

// Genes cuantitativos (alelos con efecto gradual). Los valores son relativos a la especie: 1 es
// lo típico; los organelos que la especie no tiene quedan en 0 y no pueden aparecer por
// mutación (sin endosimbiosis no se "inventa" un cloroplasto).
enum class Gene : std::uint8_t {
    Size,         // tamaño celular (dimensiones lineales)
    Motor,        // densidad de cilios / largo del flagelo / fuerza del pseudópodo
    Mitochondria, // capacidad respiratoria (ATP para crecer)
    Chloroplasts, // fotosíntesis (solo especies con cloroplastos: Euglena)
    Lysosomes,    // velocidad de digestión
    Trichocysts,  // defensa: arpones que se disparan al ser atacado (solo ciliados)
    Armor,        // lórica/testa: protege de la fagocitosis, pesa y cuesta
    Rigidity,     // rigidez de la película (memoria de forma)
    MatingType,   // tipo de apareamiento (0/1): la conjugación requiere tipos distintos
    Count
};

inline constexpr std::size_t kGeneCount = static_cast<std::size_t>(Gene::Count);

struct GeneInfo {
    const char* name;
    float min, max;
    float sigma; // tamaño típico de una mutación
};

const GeneInfo& geneInfo(Gene gene);

struct Genome {
    std::array<float, kGeneCount> v{};
    float operator[](Gene g) const { return v[static_cast<std::size_t>(g)]; }
    float& operator[](Gene g) { return v[static_cast<std::size_t>(g)]; }
};

Genome speciesGenome(LocomotionType type);
bool geneAllowed(LocomotionType type, Gene gene);

// Mitosis: cada gen permitido muta con probabilidad `rate`; el tipo de apareamiento a veces
// cambia (variación entre cariónidas, como en Paramecium).
void mutate(Genome& genome, LocomotionType type, Rng& rng, float rate = 0.25f);
// Conjugación: cada gen viene de uno u otro progenitor. Los dos exconjugantes quedan con el
// MISMO genotipo (intercambian pronúcleos haploides recíprocamente).
Genome recombine(const Genome& a, const Genome& b, Rng& rng);
// Herencia entre partidas: cada gen viene del legado con probabilidad 1/2.
Genome inherit(LocomotionType type, const Genome& legacy, Rng& rng);

// Rasgos fisiológicos que se derivan del genoma (multiplicadores sobre la especie).
struct Traits {
    float sizeScale = 1.0f;        // dimensiones lineales
    float speed = 1.0f;
    float motilityCost = 1.0f;
    float growthMax = 1.0f;
    float maintenance = 1.0f;      // incluye el costo de mantener los organelos
    float photosynthesis = 0.0f;
    float digestion = 1.0f;
    float trichocystDefense = 0.0f; // probabilidad de rechazar un ataque
    float armor = 0.0f;
    float rigidity = 1.0f;
    float volume = 1.0f;           // los organelos ocupan lugar: célula más grande
};

Traits traitsOf(const Genome& genome, LocomotionType type);

// Componente: genoma y rasgos de una célula (fijos de nacimiento a división o conjugación).
struct Heredity {
    Genome genome;
    Traits traits;
};
