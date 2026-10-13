#pragma once

#include "bio/species.hpp"

#include <cstdint>

// Modo sandbox: catálogo de todo lo que vive en la gota (células y microbios que no son
// cuerpos blandos) para spawnearlo a mano.
enum class BeingKind : std::uint8_t {
    Species,        // célula u organismo de una especie (cuerpo blando)
    Bacterium,      // bacterias genéricas (racimo)
    Violaceum,      // Chromobacterium violaceum (quorum sensing)
    Bdellovibrio,   // depredadoras de bacterias
    Cyanobacterium, // filamento de Anabaena
    Chytrid,        // zoosporas de quitridio
    Saprolegnia,    // micelio descomponedor
    Zoophagus,      // micelio que atrapa rotíferos
    Phage,          // dosis de fagos líticos (un lisado)
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

inline const char* beingName(Being b) {
    switch (b.kind) {
    case BeingKind::Species: return speciesInfo(b.species).name;
    case BeingKind::Bacterium: return "Bacterias";
    case BeingKind::Violaceum: return "C. violaceum";
    case BeingKind::Bdellovibrio: return "Bdellovibrio";
    case BeingKind::Cyanobacterium: return "Anabaena";
    case BeingKind::Chytrid: return "Quitridio";
    case BeingKind::Saprolegnia: return "Saprolegnia";
    case BeingKind::Zoophagus: return "Zoophagus";
    case BeingKind::Phage: return "Fagos";
    case BeingKind::Count: break;
    }
    return "";
}
