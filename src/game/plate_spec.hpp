#pragma once

#include "game/biome.hpp"
#include "game/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Especificación de una placa de Petri: todo lo que se preconfigura antes de crear el mundo
// (bioma base, seed, contenido, ambiente, nutrientes, sedimento y poblaciones). `config` es lo
// que recibe `World`; cambiar bioma, seed o contenido lo regenera desde el bioma (los ajustes
// a mano se pierden, como al elegir otro preset).
struct PlateSpec {
    int biome = 0;
    std::uint64_t seed = 1;
    DropContent content = DropContent::EmptyPetri;
    WorldConfig config;

    PlateSpec() { rebuild(); }
    // config = bioma + seed + contenido.
    void rebuild();
    // Población silvestre inicial que `Run::start` agrega (0 en una placa vacía).
    float wildlife() const;
};

// Texto clave=valor versionado (`plate_version=1`). Al leer, lo que falta queda como en el
// bioma, así los archivos viejos siguen valiendo cuando se agregan campos.
std::string serializePlate(const PlateSpec& spec);
bool parsePlate(const std::string& text, PlateSpec& spec);

// Carpeta de placas del usuario: <datos>/biogame/plates.
std::string defaultPlatesDir();
bool savePlate(const PlateSpec& spec, const std::string& path);
bool loadPlate(const std::string& path, PlateSpec& spec);
// Nombres (sin extensión) de las placas guardadas, en orden alfabético.
std::vector<std::string> listPlates(const std::string& dir);
