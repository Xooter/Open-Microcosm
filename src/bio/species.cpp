#include "bio/species.hpp"

namespace {

CellSpec makeSpec(float rx, float ry, float shapeRate, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  float nucleus = 0.32f) {
    CellSpec s;
    s.radiusX = rx;
    s.radiusY = ry;
    s.shapeRate = shapeRate;
    s.r = r;
    s.g = g;
    s.b = b;
    s.nucleusFraction = nucleus;
    return s;
}

const SpeciesInfo kSpecies[kSpeciesCount] = {
    // Paramecium caudatum: ~180 × 50 µm, ciliado bacterívoro.
    {"Paramecium", LocomotionType::Cilia, makeSpec(90.0f, 25.0f, 8.0f, 160, 176, 138), 1.0f,
     FeedingMode::OralGroove, 0, speciesBit(SpeciesId::Didinium), true, false, 1.0f, 1, false, 10},
    // Euglena gracilis: ~50 × 14 µm, mixótrofa con cloroplastos y estigma.
    {"Euglena", LocomotionType::Flagellum, makeSpec(25.0f, 7.0f, 4.0f, 120, 172, 92), 1.0f,
     FeedingMode::Anterior, 0, 0, false, true, 1.0f, 1, false, 10},
    // Amoeba proteus (chica): ~180 µm, fagocita algas y ciliados.
    {"Amoeba", LocomotionType::Pseudopod, makeSpec(90.0f, 90.0f, 0.0f, 188, 176, 160, 0.2f), 1.0f,
     FeedingMode::Phagocytosis,
     speciesBit(SpeciesId::Euglena) | speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Paramecium), 0,
     false, false, 1.0f, 1, false, 2},
    // Chlamydomonas reinhardtii: ~10 µm, dos flagelos, un gran cloroplasto en copa; se duplica
    // en ~6–8 h con buena luz (3× la tasa de Euglena). Se divide
    // dentro de la pared madre en 4 hijas (palintomía).
    {"Chlamydomonas", LocomotionType::Flagellum, makeSpec(5.5f, 4.5f, 6.0f, 110, 170, 80), 1.25f,
     FeedingMode::None, 0, speciesBit(SpeciesId::Amoeba), false, true, 3.0f, 2, false, 40},
    // Didinium nasutum: ~110 × 80 µm, en barril; caza paramecios con su probóscide y los traga
    // enteros. Cuando pasa hambre se enquista.
    {"Didinium", LocomotionType::Cilia, makeSpec(55.0f, 40.0f, 8.0f, 176, 168, 140), 1.0f,
     FeedingMode::Raptorial, speciesBit(SpeciesId::Paramecium), 0, false, false, 1.0f, 1, true, 3},
};

} // namespace

const SpeciesInfo& speciesInfo(SpeciesId id) { return kSpecies[static_cast<int>(id)]; }

SpeciesId playerSpecies(LocomotionType type) {
    switch (type) {
    case LocomotionType::Cilia: return SpeciesId::Paramecium;
    case LocomotionType::Flagellum: return SpeciesId::Euglena;
    case LocomotionType::Pseudopod: return SpeciesId::Amoeba;
    }
    return SpeciesId::Paramecium;
}

Genome speciesGenome(SpeciesId id) {
    Genome g = speciesGenome(speciesInfo(id).locomotion);
    switch (id) {
    case SpeciesId::Chlamydomonas: g[Gene::Chloroplasts] = 1.0f; break;
    case SpeciesId::Didinium: g[Gene::Trichocysts] = 0.0f; break; // tiene toxicistos, no tricocistos
    default: break;
    }
    return g;
}
