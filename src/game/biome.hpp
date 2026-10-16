#pragma once

#include "game/world.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Biomas (fase 11): cada uno es un ambiente real con su química, su temperatura y su elenco de
// especies. La gota de cada partida sale de un bioma con variaciones por seed. Son datos de los
// paquetes de contenido (fase 20.0): los de fábrica, en assets/content/base/biomas.toml.
// BiomeId es el índice en el registro; los nombres son alias de los de fábrica (en su orden).
enum class BiomeId : std::uint16_t { Puddle, Pond, Moss, TermiteGut, HotSpring, SeaIce };
int biomeCount();

struct Biome {
    std::string key, name, description, pack; // key: id estable de los datos ("charco")
    MediumParams medium;
    // Multiplicador de la población inicial: `baseWeight` para todos, salvo los listados por id.
    float baseWeight = 1.0f;
    std::vector<std::pair<std::string, float>> weights;
    std::size_t bacteria = 0, detritus = 0;
    int cyanoFilaments = 0;
    int oscillatoriaFilaments = 0;
    float cyanoTMin = 10.0f, cyanoTMax = 35.0f;
    int saprolegnia = 0, chytridClusters = 0;
    int allomyces = 0, lichens = 0, physarum = 0;
    float patchiness = 0.0f;
    Sediment sediment; // minerales y granos de arena (por gota de referencia)

    // Peso de un ser en este bioma. Un simbionte obligado (etiqueta `simbionte_obligado`) solo
    // está si el bioma lo nombra: Trichonympha no vive fuera del intestino de la termita.
    float weight(SpeciesId id) const;
};

// Fuera de rango devuelve el primero (con el paquete de fábrica sin cargar, un agua dulce sin
// vida: nunca hay cero biomas).
const Biome& biome(BiomeId id);
// Por id de los datos ("charco"); vacío si no está cargado.
std::optional<BiomeId> findBiome(std::string_view key);

// Carga los [[bioma]] de un archivo TOML de un paquete (se agregan a los cargados). Los biomas con
// errores se saltean y quedan en `biomeLoadWarnings`; false si el archivo no se puede leer.
bool loadBiomesFile(const std::string& path, const std::string& pack, std::string* error = nullptr);
bool loadBiomesText(const std::string& text, const std::string& source, const std::string& pack,
                    std::string* error = nullptr);
const std::vector<std::string>& biomeLoadWarnings();

// Gota procedural: el bioma con variaciones según la seed (temperatura, luz, nutrientes,
// manchas de materia orgánica y qué especies están y cuántas).
// Área de agua de la gota relativa a la de referencia (2048 × 1024 µm, con la que se
// calibraron las poblaciones de los biomas): las poblaciones se escalan para mantener la densidad.
float dropAreaScale(const WorldConfig& config);
WorldConfig makeDrop(BiomeId id, std::uint64_t seed);

// Qué hay en la gota al empezar: todo lo del bioma; solo agua, química y sedimento (sin
// poblaciones); o placa de Petri vacía.
enum class DropContent : std::uint8_t { Full, NoLife, EmptyPetri };

// Vacía la gota según `content`. Petri vacía: sin poblaciones, sin sedimento mineral (arcillas,
// limo, mica, óxidos, azufre, frústulas, granos de arena), sin polvo del aire y sin detritos; queda
// el agua con la química del bioma. Después se puede agregar cualquier cosa desde el sandbox.
void applyDropContent(WorldConfig& config, DropContent content);
