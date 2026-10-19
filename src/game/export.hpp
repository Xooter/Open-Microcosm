#pragma once

#include "game/mass_budget.hpp"
#include "game/plate_spec.hpp"
#include "game/timeseries.hpp"

#include <string>
#include <vector>

class World;
class Run;

// Exportación de datos abiertos (fase 13). Todo en texto, con unidades y los parámetros con los
// que se corrió la gota, para que otro pueda reproducirla y analizarla con sus herramientas. Los
// números se escriben con `std::to_chars`: punto decimal sin importar la configuración regional.

// Serie temporal como CSV: encabezado `nombre [unidad]`, una fila por muestra; los vacíos
// (región sin dibujar) quedan sin valor.
std::string seriesCsv(const TimeSeries& series);

// Con qué se corrió (lo que no sabe `World`): motor de XPBD, GPU y LOD. Sin esto no se puede
// decir si otra corrida debería dar lo mismo.
struct RunProvenance {
    std::string engine = "CPU"; // resolvedor de XPBD
    std::string gpu;            // GL_RENDERER, si el motor es la GPU
    bool lod = false;           // LOD activo (lo lejano con menos sub-pasos)
    std::string protocol;               // lección o protocolo en curso ("" ninguno)
    std::vector<std::string> unlocked;  // parámetros del protocolo que se destrabaron
};

// Descripción de la CPU de esta máquina ("" si no se pudo leer).
std::string machineDescription();

// Corrida completa como JSON: procedencia (versión, commit, flags, motor, máquina), todos los
// parámetros del registro (game/parameters) con su unidad, la placa con el ambiente vigente, la
// serie por columnas y el balance de masa al empezar y ahora.
std::string runJson(const World& world, const Run& run, const PlateSpec& plate, const RunProvenance& provenance = {});

// Ficha de cada especie (los datos de `SpeciesInfo` con sus unidades) como JSON.
std::string speciesJson();

// Carpeta de exportaciones: <datos>/biogame/exports.
std::string defaultExportsDir();
// Carpeta de las fotos y los time-lapses: <datos>/biogame/photos.
std::string defaultPhotosDir();
// Escribe `text` en `path` creando la carpeta. false si falla.
bool writeTextFile(const std::string& path, const std::string& text);
// Nombre de archivo base con fecha y hora local: "<prefijo>_AAAAMMDD_HHMMSS".
std::string timestampedName(const std::string& prefix);
