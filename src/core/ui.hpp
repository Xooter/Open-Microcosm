#pragma once

#include "render/field_overlay.hpp"

#include "bio/locomotion.hpp"
#include "game/sandbox.hpp"

#include <raylib.h>

#include <optional>
#include <string>
#include <vector>

class World;
class Run;
struct Progress;

struct ToolState {
    float stirRadius = 30.0f; // [µm]
    float inkRadius = 15.0f;  // [µm]
    float squeezeSpeed = 40.0f; // [µm/s]
    bool grabbing = false;
    bool follow = true;   // la cámara sigue al jugador
    bool showLab = true;  // panel de laboratorio (H para ocultar)
    bool paused = false;
    Overlay overlay = Overlay::None;
    int stain = 0;          // Stain
    // Profiling continuo (promedios suavizados) [ms].
    float stepMs = 0.0f, renderMs = 0.0f;
};

inline constexpr Rectangle kUiPanel = {10, 10, 320, 640};

// Panel de debug para tunear el medio en vivo.
void drawUi(World& world, ToolState& tools);

// Pantalla de inicio: elegir la especie fundadora (dibuja un frame completo).
struct Options;

// Pantalla de inicio (dibuja un frame completo).
enum class TitleAction { None, Play, Sandbox, Encyclopedia, Options, Quit };
TitleAction drawTitleScreen(const Progress& progress);

// Preparar la gota: bioma, especie fundadora y semilla.
struct SetupState {
    int biome = 0;
    LocomotionType founder = LocomotionType::Cilia;
    unsigned seed = 1;
    bool sandbox = false;   // modo de pruebas (todas las fundadoras, sin progreso)
    bool emptyDrop = false; // sandbox: gota sin poblaciones iniciales
};
enum class SetupAction { None, Start, Back };
SetupAction drawSetupScreen(const Progress& progress, SetupState& state);

// Enciclopedia: especies vistas y comportamientos observados. Devuelve true para volver.
bool drawEncyclopedia(const Progress& progress, float& scroll);
// Opciones (se aplican y guardan desde main). Devuelve true para volver.
bool drawOptions(Options& options);
// Avisos breves (descubrimientos) en la esquina.
void drawToasts(const std::vector<std::string>& lines, float alpha);
// Objetivo de la partida, estado del linaje y genoma (del jugador o de la candidata elegida).
void drawRunHud(const Run& run, const World& world);
// Modo sandbox: barra de íconos con todos los seres.
struct SandboxState {
    int armed = -1;       // índice del catálogo listo para spawnear (-1: ninguno)
    bool reshape = false; // modo Forma: el ícono transforma al jugador en esa especie
};
// true si el mouse está sobre la barra (los clicks no van al mundo).
bool sandboxBarHovered();
// Dibuja la barra y el fantasma del ser elegido bajo el cursor. Los clicks en los íconos
// arman el spawn o (modo Forma) cambian la especie del jugador; el botón fuerza su reproducción.
void drawSandboxBar(World& world, SandboxState& state, const Camera2D& camera);
// Fin de partida: estadísticas y desbloqueos.
void drawGameOver(const Run& run, const Progress& progress, const std::vector<LocomotionType>& unlocked);
