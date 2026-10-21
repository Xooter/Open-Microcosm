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
    int species[kMaxSpecies] = {}; // individuos vivos por especie (silvestres y del linaje)
    float biovolume[kMaxSpecies] = {}; // de esos individuos [µm³] (bio/species: cellVolumeUm3)
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
    int quorumBacteria = 0;  // C. violaceum en quorum (violetas)
    int phageInfected = 0;
    int bdelloplasts = 0, bdellovibrios = 0;
    float meanPH = 7.0f, maxPH = 7.0f;
    float meanNitrogen = 0.0f, meanToxin = 0.0f, meanPhage = 0.0f; // [µM], [viriones/µm³]
    // Medias en el agua de la placa [µM] (series de la fase 13).
    float meanOxygen = 0.0f, meanPhosphate = 0.0f, meanSilicate = 0.0f, meanIron = 0.0f, meanVitamin = 0.0f;
    float meanOrganic = 0.0f, meanCO2 = 0.0f;
    float share() const { return totalBiomass > 0.0f ? lineageBiomass / totalBiomass : 0.0f; }
};
