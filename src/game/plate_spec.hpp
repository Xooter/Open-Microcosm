#pragma once

#include "game/biome.hpp"
#include "game/prepared.hpp"
#include "game/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Especificación de una placa de Petri: todo lo que se preconfigura antes de crear el mundo
// (bioma base, seed, contenido, ambiente, nutrientes, sedimento y poblaciones). `config` es lo
// que recibe `World`; cambiar bioma, seed o contenido lo regenera desde el bioma (los ajustes
// a mano se pierden, como al elegir otro preset).
struct PlateSpec {
    // Placa preparada (plates/: un cultivo celular, por ejemplo); vacío: una gota del bioma.
    std::string prepared;
    int biome = 0;
    std::uint64_t seed = 1;
    DropContent content = DropContent::EmptyPetri;
    WorldConfig config;

    PlateSpec() { rebuild(); }
    // config = bioma + seed + contenido (o la placa preparada + seed).
    void rebuild();
    PlateKind kind() const;
    // Población silvestre inicial que `Run::start` agrega (0 en una placa vacía).
    float wildlife() const;
};

// Texto clave=valor versionado (`plate_version`). Al leer, lo que falta queda como en el
// bioma, así los archivos viejos siguen valiendo cuando se agregan campos. La versión 2 nombra
// bioma, minerales y seres por su id de texto y lista los paquetes cargados (`pack=id versión
// huella`); la 1 (índices y nombres) se sigue leyendo.
std::string serializePlate(const PlateSpec& spec);
// Lo que la placa nombra y no está cargado (un bioma o un ser de un paquete que falta, o el paquete
// mismo) va a `missing`, si se pide; la placa se lee igual con lo que hay.
bool parsePlate(const std::string& text, PlateSpec& spec, std::vector<std::string>* missing = nullptr);

// Carpeta de placas del usuario: <datos>/openmicrocosm/plates.
std::string defaultPlatesDir();
bool savePlate(const PlateSpec& spec, const std::string& path);
bool loadPlate(const std::string& path, PlateSpec& spec);
// Nombres (sin extensión) de las placas guardadas, en orden alfabético.
std::vector<std::string> listPlates(const std::string& dir);
