#pragma once

#include "bio/species.hpp"
#include "game/mineral.hpp"
#include "sim/particles.hpp"

#include <cstdint>

// Modo sandbox: catálogo de todo lo que hay en la gota (células, microbios que no son cuerpos
// blandos y el sedimento inorgánico) para spawnearlo a mano.
enum class BeingKind : std::uint8_t {
    Species,        // célula u organismo de una especie (cuerpo blando)
    Bacterium,      // bacterias genéricas (racimo)
    Violaceum,      // Chromobacterium violaceum (quorum sensing)
    Bacillus,       // Bacillus subtilis (bacilo grampositivo)
    Bdellovibrio,   // depredadoras de bacterias
    Cyanobacterium, // filamento de Anabaena
    Oscillatoria,   // tricoma de Oscillatoria
    Chytrid,        // zoosporas de quitridio
    Saprolegnia,    // micelio descomponedor
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
inline int beingCount() { return speciesCount() + static_cast<int>(BeingKind::Count) - 1; }

inline Being beingAt(int index) {
    if (index < speciesCount())
        return {BeingKind::Species, static_cast<SpeciesId>(index)};
    return {static_cast<BeingKind>(index - speciesCount() + 1), SpeciesId::Paramecium};
}

// Mineral suelto que corresponde a un ser del catálogo (kNoMineral si no es uno).
inline constexpr MineralKind kNoMineral = static_cast<MineralKind>(255);
inline MineralKind beingMineral(BeingKind k) {
    switch (k) {
    case BeingKind::Clay: return MineralKind::Clay;
    case BeingKind::Quartz: return MineralKind::Quartz;
    case BeingKind::Mica: return MineralKind::Mica;
    case BeingKind::IronOxide: return MineralKind::IronOxide;
    case BeingKind::Sulfur: return MineralKind::Sulfur;
    case BeingKind::Frustule: return MineralKind::Frustule;
    default: return kNoMineral;
    }
}

// Categorías de la barra del sandbox (una pestaña cada una). Para las especies salen de los datos
// de `SpeciesInfo`, no de código por especie.
enum class BeingCategory : std::uint8_t {
    Ciliates,    // protistas con cilios
    Flagellates, // protistas con flagelo que no son algas (Euglena, Trichonympha)
    Amoebae,     // ameboides
    Algae,       // algas unicelulares, coloniales, filamentosas y diatomeas
    Fungi,       // levaduras y hongos
    HumanCells,  // células humanas (cultivo, sangre, mucosa)
    PlantCells,  // células vegetales de los preparados
    Bacteria,    // bacterias (también cianobacterias) y fagos
    Sediment,    // minerales y granos de arena
    Count
};
inline constexpr int kCategoryCount = static_cast<int>(BeingCategory::Count);

inline BeingCategory categoryOf(Being b) {
    switch (b.kind) {
    case BeingKind::Species: {
        const SpeciesInfo& s = speciesInfo(b.species);
        if (s.animalCell.present || hasTag(b.species, "human_cell"))
            return BeingCategory::HumanCells;
        if (hasTag(b.species, "plant_cell"))
            return BeingCategory::PlantCells;
        if (s.budding && s.feeding == FeedingMode::Osmotroph)
            return BeingCategory::Fungi;
        // Liberar hijas (colonias o yemas) cuenta como alga solo con cloroplastos: las tecamebas como
        // Euglypha también brotan.
        if (s.filament || s.glider || (s.colonyDaughters > 0 && s.chloroplasts) ||
            (s.feeding == FeedingMode::None && s.chloroplasts))
            return BeingCategory::Algae;
        if (s.locomotion == LocomotionType::Pseudopod)
            return BeingCategory::Amoebae;
        if (s.locomotion == LocomotionType::Flagellum)
            return BeingCategory::Flagellates;
        return BeingCategory::Ciliates;
    }
    case BeingKind::Chytrid:
    case BeingKind::Saprolegnia:
    case BeingKind::Allomyces:
    case BeingKind::Lichen:
    case BeingKind::Physarum:
    case BeingKind::OatFlake: return BeingCategory::Fungi;
    case BeingKind::Bacterium:
    case BeingKind::Violaceum:
    case BeingKind::Bacillus:
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
    case BeingCategory::Fungi: return "Hongos y mohos";
    case BeingCategory::HumanCells: return "Células humanas";
    case BeingCategory::PlantCells: return "Células vegetales";
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
    case BeingKind::Bacillus: return "B. subtilis";
    case BeingKind::Bdellovibrio: return "Bdellovibrio";
    case BeingKind::Cyanobacterium: return "Anabaena";
    case BeingKind::Oscillatoria: return "Oscillatoria";
    case BeingKind::Chytrid: return "Quitridio";
    case BeingKind::Saprolegnia: return "Saprolegnia";
    case BeingKind::Allomyces: return "Allomyces";
    case BeingKind::Lichen: return "Hongo liquenizante";
    case BeingKind::Physarum: return "Physarum";
    case BeingKind::OatFlake: return "Copo de avena";
    case BeingKind::Phage: return "Fagos";
    case BeingKind::Clay:
    case BeingKind::Quartz:
    case BeingKind::Mica:
    case BeingKind::IronOxide:
    case BeingKind::Sulfur:
    case BeingKind::Frustule: return mineralName(beingMineral(b.kind));
    case BeingKind::Rock: return "Grano de arena";
    case BeingKind::Sediment: return "Barro (mezcla)";
    case BeingKind::Count: break;
    }
    return "";
}
