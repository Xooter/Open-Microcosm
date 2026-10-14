#pragma once

#include "bio/fungi.hpp"
#include "bio/species.hpp"
#include "core/vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

class World;
struct Progress;

// Enciclopedia (fase 11): especies vistas al microscopio y comportamientos observados. Se
// guarda en el progreso entre partidas.

// Organismos que no son especies de cuerpo blando.
enum class OtherOrganism : std::uint8_t {
    Bacteria,
    Violaceum,
    Bacillus,
    Cyanobacteria,
    Chytrid,
    Saprolegnia,
    Zoophagus,
    Phage,
    Bdellovibrio,
    Allomyces,
    Lichen,
    Physarum,
    Count
};
inline constexpr int kOtherCount = static_cast<int>(OtherOrganism::Count);

// Entrada de enciclopedia de cada hongo.
OtherOrganism otherOf(FungusKind kind);
const char* otherName(OtherOrganism o);
const char* otherBlurb(OtherOrganism o);
const char* speciesBlurb(SpeciesId id);

// Lo que el jugador ve en el campo: especies dentro de la vista y organismos ambientales.
// Devuelve los nombres de las entradas nuevas.
std::vector<std::string> discoverVisible(Progress& progress, const World& world, Vec2 viewMin, Vec2 viewMax);
// Comportamientos que ocurrieron en la gota (causas de muerte, interacciones). Devuelve los
// nuevos.
std::vector<std::string> recordBehaviors(Progress& progress, const World& world);
