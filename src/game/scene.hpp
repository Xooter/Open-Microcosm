#pragma once

#include "game/intervention.hpp"
#include "game/plate_spec.hpp"
#include "game/run.hpp"

#include <string>
#include <vector>

// Escena (MVP: reproducibilidad): una placa (.plate) más el registro de intervenciones con su
// paso. Cargarla y crear la placa repite la corrida: con el mismo binario, máquina, motor y
// opciones, bit a bit. Mismo archivo .plate: sin eventos es una placa común.
std::string serializeScene(const PlateSpec& plate, const std::vector<Intervention>& events);
// `missing`: lo que la escena nombra y no está cargado (paquetes, biomas, seres, minerales).
bool parseScene(const std::string& text, PlateSpec& plate, std::vector<Intervention>& events,
                std::vector<std::string>* missing = nullptr);
bool saveScene(const std::string& path, const PlateSpec& plate, const std::vector<Intervention>& events);
bool loadScene(const std::string& path, PlateSpec& plate, std::vector<Intervention>& events,
               std::vector<std::string>* missing = nullptr);

// Sesión de una placa como la arma el laboratorio: sin ser controlado al empezar y con la fauna
// silvestre que corresponde al contenido de la placa.
RunConfig plateRunConfig(const PlateSpec& plate);
