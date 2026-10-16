#pragma once

#include "bio/cell.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"

#include <entt/entity/entity.hpp>

#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Tipos de ser con cuerpo blando (especies de protistas, algas, levaduras y, en adelante, células
// de organismos pluricelulares). Son DATOS: se cargan de los paquetes de contenido
// (assets/content/base/seres.toml y los del usuario) en un registro; el índice en el registro es
// el SpeciesId. Los nombres de abajo son los seres de fábrica en su orden de carga (las escenas
// viejas guardan ese índice): el código que todavía distingue especies por nombre los usa.
enum class SpeciesId : std::uint16_t {
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
    Naegleria,
    Trichonympha,
    Mougeotia,
    Zygnema,
    Oedogonium,
    ParameciumAurelia, // Gause (1934): excluye a P. caudatum en cultivo mixto
    Asterionella,      // Tilman (1977): diatomeas que compiten por P y Si
    Cyclotella,
};
// Tope de tipos de ser cargados (tablas de tamaño fijo: censo, pesos de población).
inline constexpr int kMaxSpecies = 512;
// Cantidad de tipos de ser cargados (los de fábrica y los de los paquetes del usuario).
int speciesCount();
// Conjunto de tipos de ser (dietas, huidas, hospedadores).
using SpeciesSet = std::bitset<kMaxSpecies>;

// Química de las reservas y de la pared: lo que las tinciones ven (el color no sale del nombre de
// la especie sino de qué polisacárido guarda y de qué está hecha su pared).
enum class Storage : std::uint8_t {
    None,
    Starch,          // almidón verdadero en el cloroplasto (clorofitas): el yodo lo vuelve azul-negro
    Glycogen,        // glucógeno (animales, ciliados, amebas, levaduras): el yodo da marrón rojizo
    Paramylon,       // β-1,3-glucano de los euglénidos: NO reacciona con el yodo
    Chrysolaminarin, // β-glucano de las diatomeas (en una vacuola): NO reacciona con el yodo
};
enum class CellWall : std::uint8_t {
    None,        // película o membrana desnuda
    Cellulose,   // algas verdes
    Silica,      // frústula de diatomea
    YeastGlucan, // levaduras: pared gruesa de glucano y manano; se tiñe como grampositiva
};
struct SpeciesChemistry {
    Storage storage = Storage::None;
    CellWall wall = CellWall::None;
};
const SpeciesChemistry& speciesChemistry(SpeciesId id);

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
    const char* key = "";  // id estable de los datos ("paramecium"): lo guardan las escenas
    const char* name = ""; // el que se muestra
    const char* description = ""; // qué es y qué hace (ficha y enciclopedia)
    const char* plan = "";        // plan corporal (bio/body_plan, planes.toml)
    LocomotionType locomotion = LocomotionType::Cilia;
    CellSpec spec;                  // forma, tamaño y color típicos
    float speedScale = 1.0f;        // respecto de la velocidad de su tipo de locomoción
    FeedingMode feeding = FeedingMode::None;
    SpeciesSet prey;                // especies que caza, fagocita o atrapa
    SpeciesSet flee;                // especies de las que huye (reacción de evitación)
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
    SpeciesSet mouthPrey;           // células chicas que el filtrador traga con su corriente oral
    bool walker = false;            // camina sobre cirros con paradas (hipotricos: Euplotes)
    float tentacleReach = 0.0f;     // suctorios: alcance de los tentáculos desde el centro [µm]
    float neckReach = 0.0f;         // Lacrymaria: largo máximo del cuello [µm]
    // Manipulación: horas entre presas enteras (respuesta funcional de tipo II; Didinium come
    // un paramecio cada ~3 h).
    float handlingHours = 0.0f;
    // Algas.
    bool chloroplasts = false;      // fotótrofa (gen de cloroplastos activo)
    bool glider = false;            // diatomeas: se deslizan por el rafe, con inversiones
    bool vitaminB12 = false;        // auxótrofa de cobalamina: la obtiene de las bacterias
    bool frustule = false;          // sílice: cada división achica a una hija (MacDonald–Pfitzer)
    bool filament = false;          // células unidas extremo con extremo (Spirogyra)
    int filamentCells = 0;          // células por filamento al empezar
    float fragmentationPerHour = 0.0f; // probabilidad de que una unión se corte [1/h]
    // Oedogonium: fracción de los ciclos en que el protoplasto sale como una zoóspora en vez de
    // dividirse (reproducción asexual).
    float zoosporeChance = 0.0f;
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
    bool mouthAtRear = false;       // Trichonympha fagocita la madera por el polo posterior
    // Ciclosis: corriente citoplasmática que hace girar gránulos y vacuolas [µm/s]
    // (Paramecium ~2–5; en las células vegetales, como Spirogyra, a lo largo de la pared).
    float cyclosis = 0.0f;
    SpeciesChemistry chemistry;     // reserva y pared (tinciones)
    // Genes que difieren de los de su locomoción (p. ej. sin tricocistos, con testa).
    std::vector<std::pair<Gene, float>> genome;
    std::vector<std::string> tags;  // grupos (taxonómicos o funcionales): dietas, hospedadores
};

const SpeciesInfo& speciesInfo(SpeciesId id);
SpeciesId playerSpecies(LocomotionType type);
Genome speciesGenome(SpeciesId id);
// Por id de los datos ("paramecium"); vacío si no está cargado.
std::optional<SpeciesId> findSpecies(std::string_view key);
// Los tipos de ser con una etiqueta (vacío si nadie la tiene).
SpeciesSet speciesWithTag(std::string_view tag);
bool hasTag(SpeciesId id, std::string_view tag);

// Carga los [[ser]] de un archivo TOML de un paquete (se agregan al registro, después de los que
// ya hay). Las referencias (presas, huidas) se resuelven al final de cada carga, contra todo lo
// cargado. Devuelve false y deja el motivo en `error` si el archivo no se puede leer; los seres
// con errores se saltean y quedan en `speciesLoadWarnings`. El registro carga solo el paquete de
// fábrica la primera vez que se lo usa.
bool loadSpeciesFile(const std::string& path, const std::string& pack, std::string* error = nullptr);
// Lo mismo desde el texto (paquetes .biopack).
bool loadSpeciesText(const std::string& text, const std::string& source, const std::string& pack,
                     std::string* error = nullptr);
const std::vector<std::string>& speciesLoadWarnings();
// Paquete de donde vino cada ser ("base" para los de fábrica).
const std::string& speciesPack(SpeciesId id);

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

// Componente: zoóspora de alga filamentosa (Oedogonium): nada con su corona de flagelos y,
// cuando se le acaba el tiempo, se fija y crece como un filamento nuevo.
struct Zoospore {
    float hoursLeft; // [h biológicas] hasta fijarse
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
