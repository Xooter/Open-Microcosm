#pragma once

#include "bio/species.hpp"

#include <string>
#include <vector>

// Fisiología por especie con sus fuentes (MVP: validación). Vive en el archivo de cada ser
// ([physiology.<id>.<clave>], toml++): cada valor como se publicó —valor, unidad,
// temperatura de la medición y fuente— y el programa lo lleva a 20 °C con Q10 = 2 (la misma
// referencia que World::bioScale). Lo que no tiene medición directa para la especie se marca
// como estimación y se dice en la ficha.
struct SourcedValue {
    bool present = false;
    float value = 0.0f;       // en la unidad interna, a 20 °C (crecimiento: 1/h biológica)
    float published = 0.0f;   // como figura en la fuente
    std::string unit;         // la de la fuente: "1/h", "1/day", "h" o "day" (duplicación), "µM"
    float temperature = 20.0f; // de la medición [°C]
    std::string source;
    bool estimate = false;
    // Q10 propio de la medición (nado y batido: `q10` en la entrada). 0: el general del modelo.
    float q10 = 0.0f;
};

struct SpeciesPhysiology {
    SourcedValue growthMax; // tasa máxima de crecimiento de la biomasa [1/h biológica, 20 °C]
    // Medias saturaciones de Monod [µM] (sin corrección de temperatura): Tilman (1977).
    SourcedValue phosphorusHalfSaturation;
    SourcedValue siliconHalfSaturation;
    // Células de organismos pluricelulares en cultivo (fase 20b). Tasas y tiempos llevados a 20 °C
    // con Q10 = 2 (la referencia de World::bioScale: a 37 °C vuelven a su valor publicado).
    SourcedValue migrationSpeed; // rapidez del paseo persistente [µm/h]
    SourcedValue persistence;    // tiempo de persistencia de la dirección [h]
    SourcedValue phaseG1, phaseS, phaseG2, phaseM; // duración de cada fase del ciclo [h]
    SourcedValue lifespan;       // vida media sin dividirse (neutrófilo) [h]
    SourcedValue motilityHalfLife; // vida media de la motilidad de los que nadan (espermatozoide) [h]
    SourcedValue confluence;     // densidad de confluencia [células/cm²] (sin Q10)
    // Metabolismo de una célula animal (por célula; las tasas, a 20 °C por Q10).
    SourcedValue glucoseVmax;    // captación máxima de glucosa [fmol/h]
    SourcedValue glucoseKm;      // [µM] (sin Q10)
    SourcedValue glutamineVmax;  // [fmol/h]
    SourcedValue glutamineKm;    // [µM]
    SourcedValue lactatePerGlucose; // lactato secretado por glucosa captada [mol/mol] (Warburg)
    SourcedValue oxygenUptake;   // consumo de O₂ [fmol/h]
    // Alimentación por filtración (cultivo bien mezclado, fase P1): respuesta funcional de tipo II
    // con el aclaramiento a baja densidad de presas y la ingestión máxima a saturación (a 20 °C).
    SourcedValue maxIngestion; // bacterias por hora [1/h]
    SourcedValue clearance;    // agua que filtra por hora [µm³/h]
    // Nado con un motor de dineína medido en función de la temperatura (espermatozoide): la rapidez
    // media de la trayectoria [µm/s] y la frecuencia del batido [Hz], a la temperatura de la medición
    // y con su Q10 propio (`q10`), que ya incluye el cambio de viscosidad del medio. Sin conversión a
    // 20 °C: World los lleva a la temperatura del medio (swimSpeedAt, beatFrequencyAt).
    SourcedValue swimSpeed;
    SourcedValue beatFrequency;
    SourcedValue lateralAmplitude; // ALH: desplazamiento lateral de la cabeza, de pico a pico [µm]
};

// Valor de una medición con su Q10 propio a la temperatura `t` [°C] (0 si no está).
float atTemperature(const SourcedValue& v, float t);

// Las claves de cada valor ([physiology.<id>.<clave>]), para listarlos (ficha, exportación).
struct PhysiologyKey {
    const char* key;
    SourcedValue SpeciesPhysiology::*member;
    const char* unit; // la interna
    const char* label;
};
const std::vector<PhysiologyKey>& physiologyKeys();

// La de la especie (la de fábrica se carga la primera vez, de los archivos de los seres).
const SpeciesPhysiology& speciesPhysiology(SpeciesId id);
// Carga explícita (tests, otra ruta). Devuelve false y deja el motivo en `error` si falla; las
// especies sin datos quedan con `present = false` (el metabolismo usa el valor general).
bool loadSpeciesPhysiology(const std::string& path, std::string* error = nullptr);
// Vuelve a la de fábrica (descarta lo cargado después).
bool loadBasePhysiology(std::string* error = nullptr);
// Suma la de un archivo a la cargada (las tablas son [physiology.<id>.<clave>], o
// [<id>.<clave>] en un archivo suelto).
bool addSpeciesPhysiology(const std::string& path, std::string* error = nullptr);
// Lo mismo desde el texto de un paquete (el archivo de un ser o un .biopack).
bool addSpeciesPhysiologyText(const std::string& text, const std::string& source, std::string* error = nullptr);
// La fisiología del ser como se leyó, en TOML ([physiology.<id>]); "" si no tiene.
std::string physiologySource(SpeciesId id);
// Avisos de la última carga (unidades desconocidas, especies inexistentes...).
const std::vector<std::string>& physiologyWarnings();
