#pragma once

#include "bio/species.hpp"

#include <cstdint>
#include <span>

// Plan corporal: qué estructuras tiene cada tipo de ser y de qué tamaño real son. Es la jerarquía
// biológica que recorre el zoom semántico (placa > organismo > célula > estructura) y la ficha.
// Datos, no código: sumar una estructura es una fila en la tabla de su ser (más su caso en el
// render). Nada se agrega porque "se vea bien": cada fila tiene que existir en el organismo real.
//
// Fuente de cada estructura:
// - Simulated: tiene estado en la simulación (partícula, slot, campo o variable del individuo).
// - Declared: existe en el organismo real pero no se simula; el render la ubica de forma
//   determinista respecto de su ancla simulada (el núcleo, el eje de la bacteria) y solo se
//   muestra en la vista de modelo, salvo que se vea en campo claro (`brightfield`).

enum class StructureKind : std::uint8_t {
    // Envoltura celular
    PlasmaMembrane,
    OuterMembrane, // membrana externa de las gramnegativas
    Peptidoglycan, // pared de mureína (fina en gramnegativas, gruesa en grampositivas)
    CellWall,      // pared eucariota (celulosa, sílice, glucano)
    Capsule,
    // Interior y apéndices procariotas
    Nucleoid,
    Ribosomes,
    Flagellum,
    Pili,
    // Eucariotas
    Nucleus,
    Nucleolus,
    Mitochondrion,
    Chloroplast,
    Pyrenoid,
    Eyespot,
    EndoplasmicReticulum,
    Golgi,
    ContractileVacuole,
    FoodVacuole,
    Lysosome,
    StorageGranule,
    // Virus
    Capsid,
    ViralGenome,
    ViralEnvelope,
    Tail,
    Baseplate,
    TailFibers,
    Count
};

enum class StructureSource : std::uint8_t { Simulated, Declared };

// Dónde va respecto del cuerpo.
enum class Arrangement : std::uint8_t {
    Envelope,     // capa del contorno
    Interior,     // dentro del citoplasma
    Polar,        // en un polo
    Peritrichous, // repartidos sobre toda la superficie
    Perinuclear,  // junto al núcleo (RE continuo con la envoltura nuclear, Golgi)
    Anterior,     // en el polo anterior
};

struct StructureSpec {
    StructureKind kind;
    StructureSource source;
    Arrangement arrangement;
    float length;   // dimensión mayor [µm]
    float width;    // dimensión menor (diámetro o espesor) [µm]
    int count;      // por célula (0: no aplica o lo da la simulación)
    bool brightfield; // se distingue en campo claro sin teñir (además de superar la resolución)
    const char* name; // en español, ASCII (la ficha)
};

enum class Morphology : std::uint8_t {
    Rod,           // bacilo
    Vibrio,        // bastón curvo
    Eukaryote,     // célula con núcleo (la forma la da la simulación)
    HeadTailPhage, // cabeza icosaédrica + cola (Caudovirales)
    Icosahedral,   // sin cola
    Helical,
};

struct BodyPlan {
    const char* name;
    Morphology morphology;
    std::span<const StructureSpec> structures;

    const StructureSpec* find(StructureKind kind) const {
        for (const StructureSpec& s : structures)
            if (s.kind == kind)
                return &s;
        return nullptr;
    }
    bool has(StructureKind kind) const { return find(kind) != nullptr; }
};

// Límite de resolución del microscopio óptico (Abbe, d = 0,61 λ / AN con luz verde y objetivo
// de inmersión de AN 1,4): lo más chico que se separa en campo claro.
inline constexpr float kOpticalResolution = 0.24f; // [µm]

// ¿Se ve con el microscopio de la placa (campo claro, sin teñir)? Ancho 0: lo da la simulación
// (núcleo, vacuolas), siempre por encima del límite.
inline bool visibleInBrightfield(const StructureSpec& s, float resolution = kOpticalResolution) {
    return s.brightfield && (s.width == 0.0f || s.width >= resolution);
}

const char* structureName(StructureKind kind);

// Bacterias por cepa (`Bacteria::strain`: 0 bacilo gramnegativo tipo E. coli, 1 C. violaceum,
// 2 B. subtilis).
const BodyPlan& bacteriumPlan(std::uint8_t strain);
const BodyPlan& bdellovibrioPlan();

enum class VirusKind : std::uint8_t { T4Like, Count };
const BodyPlan& virusPlan(VirusKind kind);
// Fago de la simulación (los fagos líticos de `World::phage_`, tipo T4).
inline constexpr VirusKind kSimulatedPhage = VirusKind::T4Like;

// Protistas, algas y levaduras.
const BodyPlan& speciesPlan(SpeciesId id);
