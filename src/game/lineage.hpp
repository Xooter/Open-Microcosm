#pragma once

#include <entt/entity/entity.hpp>

#include "bio/species.hpp"

#include <cstdint>

// Componente: a qué linaje (partida) pertenece una célula y en qué generación nació.
struct Lineage {
    std::uint32_t run = 0;
    std::uint32_t generation = 0;
};

// Eventos que el mundo le avisa a la capa de juego (se consumen con World::takeEvents).
struct WorldEvent {
    enum class Type { PlayerDivided, PlayerDied, PlayerConjugated };
    Type type;
    entt::entity a = entt::null; // PlayerDivided: madre (sigue siendo el jugador por ahora)
    entt::entity b = entt::null; // PlayerDivided: la otra hija
    const char* cause = "";      // PlayerDied
};

// Censo de biomasa viva de la gota [pmol de materia seca].
struct Census {
    float lineageBiomass = 0.0f;
    float totalBiomass = 0.0f;
    int lineageCells = 0;
    int otherCells = 0;
    std::uint32_t maxGeneration = 0;
    int species[kSpeciesCount] = {}; // individuos vivos por especie (silvestres y del linaje)
    int bacteria = 0;
    float share() const { return totalBiomass > 0.0f ? lineageBiomass / totalBiomass : 0.0f; }
};
