#pragma once

#include "bio/fungi.hpp"
#include "bio/species.hpp"

#include <cstdint>

// Nombre y descripción de cada organismo para la ficha (qué es y qué hace).

// Organismos que no son especies de cuerpo blando.
enum class OtherOrganism : std::uint8_t {
    Bacteria,
    Violaceum,
    Bacillus,
    Cyanobacteria,
    Chytrid,
    Saprolegnia,
    Phage,
    Bdellovibrio,
    Allomyces,
    Lichen,
    Physarum,
    Count
};
inline constexpr int kOtherCount = static_cast<int>(OtherOrganism::Count);

// Entrada de cada hongo.
OtherOrganism otherOf(FungusKind kind);
const char* otherName(OtherOrganism o);
const char* otherBlurb(OtherOrganism o);
const char* speciesBlurb(SpeciesId id);
