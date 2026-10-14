#pragma once

#include "bio/genome.hpp"
#include "bio/locomotion.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

// Registro del usuario entre sesiones, guardado en disco (texto clave=valor, fácil de
// inspeccionar): lo que ya vio en la enciclopedia. No hay desbloqueos ni puntajes: todo el
// catálogo está disponible desde el primer momento.
struct Progress {
    // Enciclopedia: especies vistas (bit por SpeciesId), otros organismos (bit por
    // OtherOrganism), comportamientos observados y biomas usados (bit por BiomeId).
    std::uint64_t seenSpecies = 0;
    std::uint32_t seenOthers = 0;
    std::set<std::string> behaviors;
    std::uint32_t biomesPlayed = 0;
};

// Ruta por defecto: $XDG_DATA_HOME/biogame/progress.txt (o ~/.local/share/biogame/...).
std::string defaultProgressPath();
Progress loadProgress(const std::string& path);
bool saveProgress(const Progress& progress, const std::string& path);

// Opciones del programa (se guardan junto al progreso, en options.txt).
struct Options {
    bool fullscreen = false;
    bool postProcess = true;  // óptica del microscopio (viñeta, grano, aberración)
    bool lod = true;          // células lejanas simplificadas
    bool gpuPhysics = true;   // XPBD y contactos en la GPU (compute shaders)
    std::string language = "es"; // idioma de la interfaz (assets/lang/<código>.txt)
    float bioTimeScale = 60.0f; // compresión del tiempo biológico (1 min real = 1 h)
};
std::string defaultOptionsPath();
Options loadOptions(const std::string& path);
bool saveOptions(const Options& options, const std::string& path);
