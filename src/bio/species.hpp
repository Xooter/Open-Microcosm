#pragma once

#include "bio/cell.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"

#include <cstdint>

// Especies del ecosistema (datos, no código: el comportamiento sale de estos campos).
enum class SpeciesId : std::uint8_t { Paramecium, Euglena, Amoeba, Chlamydomonas, Didinium, Count };
inline constexpr int kSpeciesCount = static_cast<int>(SpeciesId::Count);

inline constexpr std::uint32_t speciesBit(SpeciesId s) { return 1u << static_cast<unsigned>(s); }

enum class FeedingMode : std::uint8_t {
    OralGroove,   // citostoma con corriente oral (Paramecium: bacterias y detritos)
    Anterior,     // polo anterior, poco (euglénidos)
    Phagocytosis, // contacto y copa fagocítica (Amoeba)
    Raptorial,    // traga presas enteras por el polo anterior (Didinium)
    None,         // autótrofa estricta (Chlamydomonas)
};

struct SpeciesInfo {
    const char* name;
    LocomotionType locomotion;
    CellSpec spec;           // forma, tamaño y color típicos
    float speedScale;        // respecto de la velocidad de su tipo de locomoción
    FeedingMode feeding;
    std::uint32_t preyMask;  // especies que caza o fagocita
    std::uint32_t fleeMask;  // especies de las que huye (reacción de evitación)
    bool chemokinesis;       // nada más recto cuando la comida mejora (ciliados)
    bool phototaxis;         // se orienta hacia la luz (estigma)
    float photosynthesisScale; // tasa fotosintética relativa (Chlamydomonas crece más rápido)
    int fissions;            // 1: fisión binaria; 2: palintomía en 4 hijas
    bool encysts;            // forma quistes cuando pasa hambre
    int initialCount;        // individuos al empezar una gota
};

const SpeciesInfo& speciesInfo(SpeciesId id);
SpeciesId playerSpecies(LocomotionType type);
Genome speciesGenome(SpeciesId id);

// Componente: especie de la célula.
struct Species {
    SpeciesId id;
};

// Componente: quiste (pared resistente, metabolismo mínimo, no se mueve ni come).
struct Cyst {
    float age = 0.0f; // [s]
};
