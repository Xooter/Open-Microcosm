#pragma once

#include "bio/cell.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <vector>

// Especies del ecosistema (datos, no código: el comportamiento sale de estos campos).
enum class SpeciesId : std::uint8_t {
    Paramecium,
    Euglena,
    Amoeba,
    Chlamydomonas,
    Didinium,
    Vorticella,
    Stentor,
    Podophrya,
    Euplotes,
    Arcella,
    Lacrymaria,
    Spirogyra,
    Navicula,
    Volvox,
    Pediastrum,
    Candida,
    Rotifer,
    Hydra,
    Nauplius,
    Naegleria,
    Trichonympha,
    Philodina,
    Mougeotia,
    Zygnema,
    Oedogonium,
    Chaetonotus,
    Count
};
inline constexpr int kSpeciesCount = static_cast<int>(SpeciesId::Count);

inline constexpr std::uint32_t speciesBit(SpeciesId s) { return 1u << static_cast<unsigned>(s); }

enum class FeedingMode : std::uint8_t {
    OralGroove,   // citostoma con corriente oral (Paramecium: bacterias y detritos)
    Anterior,     // polo anterior, poco (euglénidos)
    Phagocytosis, // contacto y copa fagocítica (Amoeba)
    Raptorial,    // traga presas enteras por el polo anterior (Didinium)
    Tentacles,    // tentáculos que atrapan y succionan ciliados (suctorios)
    NeckStrike,   // dispara un "cuello" extensible hacia la presa (Lacrymaria)
    Osmotroph,    // absorbe materia orgánica disuelta por la pared (levaduras)
    None,         // autótrofa estricta (Chlamydomonas)
};

struct SpeciesInfo {
    const char* name = "";
    LocomotionType locomotion = LocomotionType::Cilia;
    CellSpec spec;                  // forma, tamaño y color típicos
    float speedScale = 1.0f;        // respecto de la velocidad de su tipo de locomoción
    FeedingMode feeding = FeedingMode::None;
    std::uint32_t preyMask = 0;     // especies que caza, fagocita o atrapa
    std::uint32_t fleeMask = 0;     // especies de las que huye (reacción de evitación)
    bool chemokinesis = false;      // nada más recto cuando la comida mejora (ciliados)
    bool acidTrap = false;          // se acumula en zonas con CO₂ (Paramecium, Jennings)
    bool phototaxis = false;        // se orienta hacia la luz (estigma)
    float photosynthesisScale = 1.0f; // tasa fotosintética relativa
    int fissions = 1;               // 1: fisión binaria; 2: palintomía en 4 hijas
    bool encysts = false;           // forma quistes cuando pasa hambre
    int initialCount = 0;           // individuos al empezar una gota
    // Sésiles: viven fijas al vidrio o a un detrito por un tallo (o pie).
    bool sessile = false;
    float stalkLength = 0.0f;       // [µm]
    bool contractileStalk = false;  // espasmonema (Vorticella): se retrae en ms al ser perturbada
    std::uint32_t mouthPreyMask = 0; // células chicas que el filtrador traga con su corriente oral
    bool walker = false;            // camina sobre cirros con paradas (hipotricos: Euplotes)
    float tentacleReach = 0.0f;     // suctorios: alcance de los tentáculos desde el centro [µm]
    float neckReach = 0.0f;         // Lacrymaria: largo máximo del cuello [µm]
    // Manipulación: horas entre presas enteras (respuesta funcional de tipo II; Didinium come
    // un paramecio cada ~3 h).
    float handlingHours = 0.0f;
    // Algas.
    bool chloroplasts = false;      // fotótrofa (gen de cloroplastos activo)
    bool glider = false;            // diatomeas: se deslizan por el rafe, con inversiones
    bool frustule = false;          // sílice: cada división achica a una hija (MacDonald–Pfitzer)
    bool filament = false;          // células unidas extremo con extremo (Spirogyra)
    int filamentCells = 0;          // células por filamento al empezar
    float fragmentationPerHour = 0.0f; // probabilidad de que una unión se corte [1/h]
    int colonyDaughters = 0;        // colonias hijas que libera (Volvox, Pediastrum)
    bool motherDiesAtRelease = false; // Volvox: las hijas nacen rompiendo a la madre
    // Materia seca relativa a una célula maciza del mismo tamaño: Volvox es una esfera hueca
    // de matriz gelatinosa (~3% de células); Spirogyra tiene una gran vacuola central.
    float dryMassScale = 1.0f;
    bool budding = false;           // levaduras: la hija brota de la madre (gemación)
    // Rango de pH tolerado (fuera de él el mantenimiento se encarece).
    float phMin = 6.0f, phMax = 9.5f;
    // Temperatura [°C] y salinidad máxima [PSU] toleradas; anaerobias estrictas (el O₂ las daña).
    float tMin = 5.0f, tMax = 32.0f;
    float salinityMax = 5.0f;
    bool anaerobe = false;
    // Metazoos (fase 9): cuerpos de muchas células.
    bool multicellular = false;
    bool compound = false;          // rotífero: corona + tronco + pie unidos por adhesiones
    float maintenanceScale = 1.0f;  // metabolismo por masa (Kleiber: los grandes gastan menos)
    float mouthReach = 0.0f;        // alcance de la boca/corona [µm] (0: el de los ciliados)
    float eggHours = 0.0f;          // rotífero: incubación del huevo partenogenético
    int tentacles = 0;              // Hydra: tentáculos con nematocistos
    float tentacleLength = 0.0f;    // [µm]
    bool hopper = false;            // nauplio: saltos bruscos y pausas
    bool mouthAtRear = false;       // Trichonympha fagocita la madera por el polo posterior
    // Ciclosis: corriente citoplasmática que hace girar gránulos y vacuolas [µm/s]
    // (Paramecium ~2–5; en las células vegetales, como Spirogyra, a lo largo de la pared).
    float cyclosis = 0.0f;
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

// Componente: célula sésil fija por un tallo a un punto (`holdfast`).
struct Anchored {
    Vec2 holdfast;
    float stalk;          // largo actual del tallo [µm]
    float contracted = 0.0f; // 1 = recién contraída (espasmonema); vuelve a 0 en segundos
};

// Componente: hija nadadora de una especie sésil (telotroco), antes de fijarse.
struct Telotroch {
    float timer; // [s] hasta fijarse
};

// Componente: presa atrapada por los tentáculos de un suctorio, que le succiona el citoplasma.
struct Captured {
    entt::entity by;
};

// Componente: cuello extensible de Lacrymaria.
struct Neck {
    float length = 0.0f;   // [µm]
    Vec2 direction{1.0f, 0.0f};
    entt::entity target = entt::null;
};

// ---------------------------------------------------------------------------
// Metazoos (fase 9)
// ---------------------------------------------------------------------------

// Componente: parte de un cuerpo compuesto (corona o pie del rotífero). No tiene metabolismo
// propio: el organismo es la entidad `organism`.
struct Segment {
    entt::entity organism;
};

// Componente: partes de un cuerpo compuesto (se destruyen con el organismo).
struct Compound {
    std::vector<entt::entity> parts;
};

// Componente: huevo partenogenético (rotífero), pegado a la madre hasta eclosionar.
struct Egg {
    Genome genome;
    SpeciesId species;
    float hoursLeft;
    float biomass;      // [pmol]
    std::uint32_t run;  // linaje de la madre
    std::uint32_t generation;
};

// Hydra: cada tentáculo es una cadena cinemática que ondula; los nematocistos paralizan a la
// presa que lo toca y el tentáculo se contrae hasta llevarla a la boca.
struct Tentacle {
    float baseAngle = 0.0f;   // respecto del eje del cuerpo
    float extension = 1.0f;   // fracción del largo máximo
    float phase = 0.0f;
    entt::entity prey = entt::null;
    int preyNode = 0;
    std::vector<Vec2> nodes;  // de la base a la punta [µm]
};
struct Tentacles {
    std::vector<Tentacle> arms;
};

// Componente: presa paralizada por los nematocistos de una Hydra.
struct Stung {
    entt::entity by;
    Vec2 point; // donde la sujeta el tentáculo
};
