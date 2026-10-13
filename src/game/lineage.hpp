#pragma once

#include <entt/entity/entity.hpp>

#include "bio/fungi.hpp"
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
    int cyanoCells = 0;
    int mycelia[kFungusKinds] = {}; // por FungusKind
    int hyphaNodes = 0;
    int zoospores = 0;
    int restingSporangia = 0; // Allomyces
    int lichenized = 0;       // algas unidas a un hongo liquenizante
    int plasmodia = 0, plasmodiumNodes = 0, plasmodiumEdges = 0, sclerotia = 0, oatFlakes = 0; // Physarum
    float plasmodiumLength = 0.0f; // [µm] de tubos
    int infectedAlgae = 0;   // con quitridios
    int eggs = 0;
    int quorumBacteria = 0;  // C. violaceum en quorum (violetas)
    int phageInfected = 0;
    int bdelloplasts = 0, bdellovibrios = 0;
    float meanPH = 7.0f, maxPH = 7.0f;
    float meanNitrogen = 0.0f, meanToxin = 0.0f, meanPhage = 0.0f; // [µM], [viriones/µm³]
    float share() const { return totalBiomass > 0.0f ? lineageBiomass / totalBiomass : 0.0f; }
};
