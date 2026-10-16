#pragma once

// Lectura del medio y del sedimento desde los datos de un paquete (biomas, y en la fase 20a los
// medios de cultivo), con las mismas claves en todos lados. Solo para los .cpp de biosim.

#include "core/toml_reader.hpp"
#include "game/world.hpp"

#include <string>
#include <vector>

// Lee las claves presentes de una tabla [medio] sobre `m` (lo que falta queda como estaba) y
// avisa de las desconocidas (en `warnings`); los valores inválidos van a `errors`.
void readMedium(const toml::table& t, const std::string& where, MediumParams& m, std::vector<std::string>& errors,
                std::vector<std::string>& warnings);
// Lo mismo para una tabla [sedimento].
void readSediment(const toml::table& t, const std::string& where, Sediment& s, std::vector<std::string>& errors,
                  std::vector<std::string>& warnings);
