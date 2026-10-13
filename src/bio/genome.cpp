#include "bio/genome.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr GeneInfo kGenes[kGeneCount] = {
    {"tamano", 0.7f, 1.4f, 0.05f},
    {"motor", 0.5f, 2.0f, 0.1f},
    {"mitocondrias", 0.5f, 2.0f, 0.1f},
    {"cloroplastos", 0.0f, 2.0f, 0.1f},
    {"lisosomas", 0.5f, 2.0f, 0.1f},
    {"tricocistos", 0.0f, 2.0f, 0.15f},
    {"coraza", 0.0f, 1.0f, 0.1f},
    {"rigidez", 0.3f, 2.0f, 0.1f},
    {"apareamiento", 0.0f, 1.0f, 0.0f},
};

} // namespace

const GeneInfo& geneInfo(Gene gene) { return kGenes[static_cast<std::size_t>(gene)]; }

bool geneAllowed(LocomotionType type, Gene gene) {
    switch (gene) {
    case Gene::Chloroplasts: return type == LocomotionType::Flagellum;    // Euglena
    case Gene::Trichocysts: return type == LocomotionType::Cilia;         // Paramecium
    case Gene::Armor: return type == LocomotionType::Flagellum || type == LocomotionType::Pseudopod; // Trachelomonas, Arcella
    case Gene::Rigidity: return type != LocomotionType::Pseudopod;        // la ameba no tiene película
    default: return true;
    }
}

Genome speciesGenome(LocomotionType type) {
    Genome g;
    for (std::size_t i = 0; i < kGeneCount; ++i)
        g.v[i] = 1.0f;
    g[Gene::Chloroplasts] = type == LocomotionType::Flagellum ? 1.0f : 0.0f;
    g[Gene::Trichocysts] = type == LocomotionType::Cilia ? 1.0f : 0.0f;
    g[Gene::Armor] = 0.0f;
    g[Gene::MatingType] = 0.0f;
    return g;
}

void mutate(Genome& genome, LocomotionType type, Rng& rng, float rate) {
    for (std::size_t i = 0; i < kGeneCount; ++i) {
        const auto gene = static_cast<Gene>(i);
        if (gene == Gene::MatingType) {
            if (rng.uniform() < 0.1f)
                genome.v[i] = 1.0f - genome.v[i];
            continue;
        }
        if (!geneAllowed(type, gene) || rng.uniform() >= rate)
            continue;
        const GeneInfo& info = geneInfo(gene);
        genome.v[i] = std::clamp(genome.v[i] + info.sigma * rng.normal(), info.min, info.max);
    }
}

Genome recombine(const Genome& a, const Genome& b, Rng& rng) {
    Genome child;
    for (std::size_t i = 0; i < kGeneCount; ++i)
        child.v[i] = rng.uniform() < 0.5f ? a.v[i] : b.v[i];
    child[Gene::MatingType] = rng.uniform() < 0.5f ? 0.0f : 1.0f;
    return child;
}

Genome inherit(LocomotionType type, const Genome& legacy, Rng& rng) {
    Genome g = speciesGenome(type);
    for (std::size_t i = 0; i < kGeneCount; ++i)
        if (geneAllowed(type, static_cast<Gene>(i)) && rng.uniform() < 0.5f)
            g.v[i] = std::clamp(legacy.v[i], geneInfo(static_cast<Gene>(i)).min, geneInfo(static_cast<Gene>(i)).max);
    return g;
}

Traits traitsOf(const Genome& g, LocomotionType type) {
    Traits t;
    const float armor = g[Gene::Armor];
    t.sizeScale = g[Gene::Size];
    // Más cilios (o flagelo más largo) → más rápido, con rendimiento decreciente, y más caro.
    t.speed = std::sqrt(g[Gene::Motor]) * (1.0f - 0.4f * armor);
    // Un solo flagelo empuja igual sin importar el tamaño; el arrastre crece con él.
    if (type == LocomotionType::Flagellum)
        t.speed /= g[Gene::Size];
    t.motilityCost = g[Gene::Motor];
    t.growthMax = (0.6f + 0.4f * g[Gene::Mitochondria]) * (1.0f - 0.3f * armor);
    t.photosynthesis = g[Gene::Chloroplasts];
    t.digestion = g[Gene::Lysosomes];
    t.trichocystDefense = g[Gene::Trichocysts] / (g[Gene::Trichocysts] + 1.0f);
    t.armor = armor;
    t.rigidity = g[Gene::Rigidity];
    // Mantener organelos cuesta, y ocupan lugar.
    const Genome base = speciesGenome(type);
    const float extra = (g[Gene::Mitochondria] - base[Gene::Mitochondria]) +
                        (g[Gene::Chloroplasts] - base[Gene::Chloroplasts]) +
                        (g[Gene::Lysosomes] - base[Gene::Lysosomes]);
    t.maintenance = 1.0f + 0.15f * (g[Gene::Mitochondria] - 1.0f) + 0.05f * (g[Gene::Lysosomes] - 1.0f) +
                    0.05f * g[Gene::Trichocysts] + 0.2f * armor;
    t.volume = std::max(0.8f, 1.0f + 0.05f * extra);
    return t;
}
