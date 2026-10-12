#pragma once

#include "bio/locomotion.hpp"

#include <string>
#include <vector>

// Meta-progresión entre partidas, guardada en disco (texto clave=valor, fácil de inspeccionar).
// La herencia de rasgos genéticos entre partidas llega con el genoma (fase 6).
struct Progress {
    bool unlocked[3] = {true, false, false}; // cilios, flagelo, pseudópodo
    int runs = 0;
    int wins = 0;
    std::uint32_t bestGeneration = 0;
    float bestShare = 0.0f;

    bool isUnlocked(LocomotionType t) const { return unlocked[static_cast<int>(t)]; }

    // Registra una partida terminada. Devuelve las especies desbloqueadas por ella.
    //  - Euglena (flagelo): llegar a la generación 3.
    //  - Amoeba (pseudópodo): llegar a la generación 5 o ganar una partida.
    std::vector<LocomotionType> recordRun(std::uint32_t generation, float share, bool won);
};

// Ruta por defecto: $XDG_DATA_HOME/biogame/progress.txt (o ~/.local/share/biogame/...).
std::string defaultProgressPath();
Progress loadProgress(const std::string& path);
bool saveProgress(const Progress& progress, const std::string& path);
