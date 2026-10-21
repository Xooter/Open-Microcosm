#pragma once

#include "core/vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

class World;
class ChemField;

// Intervenciones del usuario sobre la placa (MVP: reproducibilidad). Toda acción que cambia el
// mundo pasa por acá: se aplica entre dos pasos y se anota con el número de paso, así una escena o
// un replay la repiten en el mismo lugar de la trayectoria. Las de mantener apretado (tinta,
// gotero) son "pinceles": un estado que se aplica una vez por paso, no por cuadro (antes el
// resultado dependía de los FPS).
enum class InterventionKind : std::uint8_t {
    Parameter,     // key = clave del registro (game/parameters), v[0] = valor
    Medium,        // v = temperatura, salinidad, luz, intercambio de O2, alcalinidad, anoxia (0/1)
    Concentration, // index = soluto (soluteField), v[0] = concentración media objetivo
    Detritus,      // pos
    Erase,         // pos
    Lyse,          // pos
    Spawn,         // pos, index = BeingKind, v[0] = SpeciesId, key = id de texto del ser (el
                   // índice cambia con los paquetes cargados: al leer manda el id)
    Ink,           // pincel: active, pos, v[0] = radio
    Dropper,       // pincel: active, pos, index = soluto, v[0] = radio, v[1] = cantidad relativa
    Stir,          // estado: active, pos, v[0..1] = velocidad, v[2] = radio
    Squeeze,       // estado: active, pos, v[0] = rapidez
    GrabBegin,     // pos
    GrabMove,      // pos
    GrabEnd,
    PlayerPick,    // pos, v[0] = tolerancia: controla la célula con locomoción que esté ahí
    PlayerInput,   // v = adelante, giro, batir (0/1), pseudópodo (0/1); pos = puntería
    PlayerSpecies, // pos (respaldo), index = especie, key = su id: el ser controlado pasa a ser de esa especie
    Fix,           // fijar la muestra
    Focus,         // estado: pos = centro de la vista, v[0..1] = medio ancho y alto [µm] (LOD)
    Engine,        // marca: index = 0 CPU, 1 GPU (la aplica quien reproduce, si puede)
    Atmosphere,    // v[0] = fracción de CO₂ del aire (incubadora), v[1] = pK′ del carbonato del medio,
                   // v[2] = osmolitos agregados [mOsm/L] (sacarosa)
    Inoculate,     // index = BeingKind, v[0] = SpeciesId, key = su id, v[1] = cantidad, v[2] = radio
                   // [µm] alrededor de pos (0: toda la placa)
    Program,       // index 0: agrega la entrada `key` (texto de game/program con comas en vez de
                   // espacios: la clave es una sola palabra) al programa de la placa; 1: lo vacía;
                   // 2: quita las entradas del objetivo `key` (p. ej. "dilution")
    Thin,          // v[0] = fracción de la gota que se saca y se repone con medio nuevo (World::thin)
    Count
};

struct Intervention {
    std::uint64_t step = 0; // se aplica antes de este paso
    InterventionKind kind = InterventionKind::Parameter;
    bool active = false;    // pinceles y estados: encendido o apagado
    int index = 0;
    Vec2 pos;
    float v[6] = {};
    std::string key;
};

// Campo de cada soluto que se puede gotear o fijar: 0 P, 1 N, 2 Si, 3 Fe, 4 B12, 5 materia orgánica,
// 6 CO2, 7 reactivo (el que nombra la clave del gotero: game/reagent).
ChemField& soluteField(World& world, int index);
int soluteCount();
const char* soluteName(int index);
float soluteTypical(int index); // concentración típica de una gota [µM]

// Aplica una intervención puntual o cambia un estado. Devuelve si tuvo efecto (agarrar algo,
// controlar una célula).
bool applyIntervention(World& world, const Intervention& iv);

// Registro de una sesión: lo aplicado (con su paso) y los pinceles activos.
class InterventionLog {
  public:
    void clear();
    // Aplica ya (entre pasos) y anota con el paso que viene.
    bool record(World& world, Intervention iv);
    // Para estados que la interfaz repite en cada cuadro (revolver, apretar, pinceles, entrada del
    // jugador): solo se anota y aplica si cambió respecto del último de su tipo.
    void recordState(World& world, const Intervention& iv);
    // Antes de cada paso: lo programado para este paso (escena o replay) y los pinceles.
    void beforeStep(World& world, float dt);
    // Programa intervenciones para reproducirlas (cada una en su paso).
    void schedule(std::vector<Intervention> script);
    const std::vector<Intervention>& entries() const { return entries_; }
    // Huella del registro (para la procedencia de una exportación).
    std::uint64_t hash() const;

  private:
    void track(const Intervention& iv);
    std::vector<Intervention> entries_;
    std::vector<Intervention> script_;
    std::size_t scriptNext_ = 0;
    Intervention ink_, dropper_; // pinceles (active = false: apagados)
    Intervention lastState_[static_cast<int>(InterventionKind::Count)];
    bool hasState_[static_cast<int>(InterventionKind::Count)] = {};
};

// Texto: una intervención por línea, `event=paso tipo activo índice x y v0..v5 [clave]`.
std::string serializeInterventions(const std::vector<Intervention>& list);
// Los seres van por id de texto; un evento con un ser que no está cargado se descarta y su id va a
// `missing` (si se pide).
std::vector<Intervention> parseInterventions(const std::string& text, std::vector<std::string>* missing = nullptr);
