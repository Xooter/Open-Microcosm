#pragma once

#include "game/mass_budget.hpp"
#include "game/plate_spec.hpp"
#include "game/timeseries.hpp"

#include <string>

class World;
class Run;

// Exportación de datos abiertos (fase 13). Todo en texto, con unidades y los parámetros con los
// que se corrió la gota, para que otro pueda reproducirla y analizarla con sus herramientas. Los
// números se escriben con `std::to_chars`: punto decimal sin importar la configuración regional.

// Serie temporal como CSV: encabezado `nombre [unidad]`, una fila por muestra; los vacíos
// (región sin dibujar) quedan sin valor.
std::string seriesCsv(const TimeSeries& series);

// Corrida completa como JSON: metadatos, parámetros (placa con el ambiente vigente, metabolismo,
// escala de tiempo), la serie por columnas y el balance de masa al empezar y ahora.
std::string runJson(const World& world, const Run& run, const PlateSpec& plate);

// Ficha de cada especie (los datos de `SpeciesInfo` con sus unidades) como JSON.
std::string speciesJson();

// Carpeta de exportaciones: <datos>/biogame/exports.
std::string defaultExportsDir();
// Escribe `text` en `path` creando la carpeta. false si falla.
bool writeTextFile(const std::string& path, const std::string& text);
// Nombre de archivo base con fecha y hora local: "<prefijo>_AAAAMMDD_HHMMSS".
std::string timestampedName(const std::string& prefix);
