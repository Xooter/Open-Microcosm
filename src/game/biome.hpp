#pragma once

#include "game/world.hpp"

#include <cstdint>

// Biomas (fase 11): cada uno es un ambiente real con su química, su temperatura y su elenco de
// especies. La gota de cada partida sale de un bioma con variaciones por seed.
enum class BiomeId : std::uint8_t { Puddle, Pond, Moss, TermiteGut, HotSpring, SeaIce, Count };
inline constexpr int kBiomeCount = static_cast<int>(BiomeId::Count);

struct Biome {
    const char* name;
    const char* description;
    MediumParams medium;
    float weight[kSpeciesCount]; // multiplicador de la población inicial de cada especie
    std::size_t bacteria, detritus;
    int cyanoFilaments;
    int oscillatoriaFilaments;
    float cyanoTMin, cyanoTMax;
    int saprolegnia, zoophagus, chytridClusters;
    int allomyces, lichens, physarum;
    float patchiness;
    Sediment sediment; // minerales y granos de arena (por gota de referencia)
};

const Biome& biome(BiomeId id);

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
