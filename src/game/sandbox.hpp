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
