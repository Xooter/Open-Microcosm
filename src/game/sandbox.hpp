#pragma once

#include "bio/species.hpp"
#include "sim/particles.hpp"

#include <cstdint>

// Modo sandbox: catálogo de todo lo que hay en la gota (células, microbios que no son cuerpos
// blandos y el sedimento inorgánico) para spawnearlo a mano.
enum class BeingKind : std::uint8_t {
    Species,        // célula u organismo de una especie (cuerpo blando)
    Bacterium,      // bacterias genéricas (racimo)
    Violaceum,      // Chromobacterium violaceum (quorum sensing)
    Bdellovibrio,   // depredadoras de bacterias
    Cyanobacterium, // filamento de Anabaena
    Oscillatoria,   // tricoma de Oscillatoria
    Chytrid,        // zoosporas de quitridio
    Saprolegnia,    // micelio descomponedor
    Zoophagus,      // micelio que atrapa rotíferos
    Allomyces,      // micelio dicotómico con esporangios de resistencia
    Lichen,         // hongo liquenizante: se une a algas verdes
    Physarum,       // plasmodio de moho mucilaginoso (no es un hongo)
    OatFlake,       // copo de avena: cebo para el plasmodio
    Phage,          // dosis de fagos líticos (un lisado)
    // Sedimento (no vive): partículas minerales sueltas, un grano de arena y barro mezclado.
    Clay,
    Quartz,
    Mica,
    IronOxide,
    Sulfur,
    Frustule,
    Rock,
    Sediment,
    Count
};

struct Being {
    BeingKind kind = BeingKind::Species;
    SpeciesId species = SpeciesId::Paramecium;
};

// Primero las especies (en el orden de SpeciesId), después los microbios.
inline constexpr int kBeingCount = kSpeciesCount + static_cast<int>(BeingKind::Count) - 1;

inline Being beingAt(int index) {
    if (index < kSpeciesCount)
        return {BeingKind::Species, static_cast<SpeciesId>(index)};
    return {static_cast<BeingKind>(index - kSpeciesCount + 1), SpeciesId::Paramecium};
}

// Mineral suelto que corresponde a un ser del catálogo (Count si no es uno).
inline MineralKind beingMineral(BeingKind k) {
    switch (k) {
    case BeingKind::Clay: return MineralKind::Clay;
    case BeingKind::Quartz: return MineralKind::Quartz;
    case BeingKind::Mica: return MineralKind::Mica;
    case BeingKind::IronOxide: return MineralKind::IronOxide;
    case BeingKind::Sulfur: return MineralKind::Sulfur;
    case BeingKind::Frustule: return MineralKind::Frustule;
    default: return MineralKind::Count;
    }
}

// Categorías de la barra del sandbox (una pestaña cada una). Para las especies salen de los datos
// de `SpeciesInfo`, no de código por especie.
enum class BeingCategory : std::uint8_t {
    Ciliates,    // protistas con cilios
    Flagellates, // protistas con flagelo que no son algas (Euglena, Trichonympha)
    Amoebae,     // ameboides
    Algae,       // algas unicelulares, coloniales, filamentosas y diatomeas
    Animals,     // metazoos: rotíferos, Hydra, nauplios, gastrotricos y nematodos
    Fungi,       // levaduras y hongos
    Bacteria,    // bacterias (también cianobacterias) y fagos
    Sediment,    // minerales y granos de arena
    Count
};
inline constexpr int kCategoryCount = static_cast<int>(BeingCategory::Count);

inline BeingCategory categoryOf(Being b) {
    switch (b.kind) {
    case BeingKind::Species: {
        const SpeciesInfo& s = speciesInfo(b.species);
        if (s.multicellular)
            return BeingCategory::Animals;
        if (s.budding && s.feeding == FeedingMode::Osmotroph)
            return BeingCategory::Fungi;
        if (s.filament || s.glider || s.colonyDaughters > 0 || (s.feeding == FeedingMode::None && s.chloroplasts))
            return BeingCategory::Algae;
        if (s.locomotion == LocomotionType::Pseudopod)
            return BeingCategory::Amoebae;
        if (s.locomotion == LocomotionType::Flagellum)
            return BeingCategory::Flagellates;
        return BeingCategory::Ciliates;
    }
    case BeingKind::Chytrid:
    case BeingKind::Saprolegnia:
    case BeingKind::Zoophagus:
    case BeingKind::Allomyces:
    case BeingKind::Lichen:
    case BeingKind::Physarum:
    case BeingKind::OatFlake: return BeingCategory::Fungi;
    case BeingKind::Bacterium:
    case BeingKind::Violaceum:
    case BeingKind::Bdellovibrio:
    case BeingKind::Cyanobacterium:
    case BeingKind::Oscillatoria:
    case BeingKind::Phage: return BeingCategory::Bacteria;
    case BeingKind::Clay:
    case BeingKind::Quartz:
    case BeingKind::Mica:
    case BeingKind::IronOxide:
    case BeingKind::Sulfur:
    case BeingKind::Frustule:
    case BeingKind::Rock:
    case BeingKind::Sediment: return BeingCategory::Sediment;
    case BeingKind::Count: break;
    }
    return BeingCategory::Sediment;
}

inline const char* categoryName(BeingCategory c) {
    switch (c) {
    case BeingCategory::Ciliates: return "Ciliados";
    case BeingCategory::Flagellates: return "Flagelados";
    case BeingCategory::Amoebae: return "Amebas";
    case BeingCategory::Algae: return "Algas";
    case BeingCategory::Animals: return "Animales";
    case BeingCategory::Fungi: return "Hongos y mohos";
    case BeingCategory::Bacteria: return "Bacterias y virus";
    case BeingCategory::Sediment: return "Sedimento";
    case BeingCategory::Count: break;
    }
    return "";
}

inline const char* beingName(Being b) {
    switch (b.kind) {
    case BeingKind::Species: return speciesInfo(b.species).name;
    case BeingKind::Bacterium: return "Bacterias";
    case BeingKind::Violaceum: return "C. violaceum";
    case BeingKind::Bdellovibrio: return "Bdellovibrio";
    case BeingKind::Cyanobacterium: return "Anabaena";
    case BeingKind::Oscillatoria: return "Oscillatoria";
    case BeingKind::Chytrid: return "Quitridio";
    case BeingKind::Saprolegnia: return "Saprolegnia";
    case BeingKind::Zoophagus: return "Zoophagus";
    case BeingKind::Allomyces: return "Allomyces";
    case BeingKind::Lichen: return "Hongo liquenizante";
    case BeingKind::Physarum: return "Physarum";
    case BeingKind::OatFlake: return "Copo de avena";
    case BeingKind::Phage: return "Fagos";
    case BeingKind::Clay: return "Arcilla";
    case BeingKind::Quartz: return "Limo de cuarzo";
    case BeingKind::Mica: return "Mica";
    case BeingKind::IronOxide: return "Oxido de hierro";
    case BeingKind::Sulfur: return "Azufre";
    case BeingKind::Frustule: return "Frustula vacia";
    case BeingKind::Rock: return "Grano de arena";
    case BeingKind::Sediment: return "Barro (mezcla)";
    case BeingKind::Count: break;
    }
    return "";
}
