#pragma once

#include "bio/genome.hpp"
#include "bio/locomotion.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

// Meta-progresión entre partidas, guardada en disco (texto clave=valor, fácil de inspeccionar).
struct Progress {
    bool unlocked[3] = {true, false, false}; // cilios, flagelo, pseudópodo
    int runs = 0;
    int wins = 0;
    std::uint32_t bestGeneration = 0;
    float bestShare = 0.0f;
    // Legado genético: el genoma del último linaje de cada especie. La próxima partida con esa
    // especie hereda cada gen con probabilidad 1/2 (ver inherit).
    bool hasLegacy[3] = {false, false, false};
    Genome legacy[3];
    // Enciclopedia: especies vistas (bit por SpeciesId), otros organismos (bit por
    // OtherOrganism), comportamientos observados y biomas jugados (bit por BiomeId).
    std::uint64_t seenSpecies = 0;
    std::uint32_t seenOthers = 0;
    std::set<std::string> behaviors;
    std::uint32_t biomesPlayed = 0;

    bool isUnlocked(LocomotionType t) const { return unlocked[static_cast<int>(t)]; }
    Genome founderGenome(LocomotionType t, Rng& rng) const;

    // Registra una partida terminada. Devuelve las especies desbloqueadas por ella.
    //  - Euglena (flagelo): llegar a la generación 3.
    //  - Amoeba (pseudópodo): llegar a la generación 5 o ganar una partida.
    std::vector<LocomotionType> recordRun(std::uint32_t generation, float share, bool won,
                                          LocomotionType founder, const Genome& lineageGenome);
};

// Ruta por defecto: $XDG_DATA_HOME/biogame/progress.txt (o ~/.local/share/biogame/...).
std::string defaultProgressPath();
Progress loadProgress(const std::string& path);
bool saveProgress(const Progress& progress, const std::string& path);

// Opciones del juego (se guardan junto al progreso, en options.txt).
struct Options {
    bool fullscreen = false;
    bool postProcess = true;  // óptica del microscopio (viñeta, grano, aberración)
    bool lod = true;          // células lejanas simplificadas
    bool gpuPhysics = true;   // XPBD y contactos en la GPU (compute shaders)
    bool labPanel = true;     // panel de laboratorio al empezar
    float bioTimeScale = 60.0f; // compresión del tiempo biológico (1 min real = 1 h)
};
std::string defaultOptionsPath();
Options loadOptions(const std::string& path);
bool saveOptions(const Options& options, const std::string& path);
