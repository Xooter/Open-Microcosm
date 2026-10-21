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
    float min, max; // límites fisiológicos (relativos a la especie)
};

const GeneInfo& geneInfo(Gene gene);

struct Genome {
    std::array<float, kGeneCount> v{};
    float operator[](Gene g) const { return v[static_cast<std::size_t>(g)]; }
    float& operator[](Gene g) { return v[static_cast<std::size_t>(g)]; }
};

Genome speciesGenome(LocomotionType type);
bool geneAllowed(LocomotionType type, Gene gene);

// Variación genética de una especie (datos: [being.genetics]). Los valores de fábrica salen de la
// genética cuantitativa (docs/mechanics/genome-and-organelles.md):
//  - `mutationalCv`: lo que la mutación agrega por generación a cada gen, como coeficiente de
//    variación (≈0,5 % en rasgos morfológicos: Houle, Morikawa y Lynch 1996, Genetics 143: 1467).
//    ESTIMACIÓN: no hay mediciones en protistas.
//  - `standingCv`: la variación de una población silvestre. La varianza genética que se mantiene es
//    ~100 generaciones de mutación en rasgos morfológicos (VG/VM ≈ 100, Houle et al. 1996):
//    √100 × 0,5 % = 5 %.
//  - `macronucleus`: ciliados. El fenotipo lo da el macronúcleo, amitótico y muy poliploide; las
//    mutaciones nuevas quedan en el micronúcleo (línea germinal) y se expresan recién después del
//    sexo, así que la fisión asexual no cambia el genoma expresado.
struct GeneticsSpec {
    float mutationalCv = 0.005f;
    float standingCv = 0.05f;
    bool macronucleus = false;
};

// División: cada gen permitido cambia por N(0, CVm²·escala) (relativo al valor 1 de la especie),
// acotado a sus límites. `mutationScale` multiplica la varianza mutacional (1 = la publicada; más
// es una evolución acelerada a propósito). El tipo de apareamiento no cambia en la fisión (en
// Paramecium se fija al formarse el macronúcleo).
void mutateDivision(Genome& genome, LocomotionType type, const GeneticsSpec& genetics, float mutationScale, Rng& rng);
// Un individuo de una población silvestre: cada gen permitido se aparta de la especie por
// N(0, cv²) (cv = standingCv × la escala que pida el usuario; 0 = un clon).
void standingVariation(Genome& genome, LocomotionType type, float cv, Rng& rng);
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
