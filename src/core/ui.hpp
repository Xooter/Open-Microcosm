#pragma once

#include "render/field_overlay.hpp"

#include "bio/locomotion.hpp"

#include <raylib.h>

#include <optional>
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
};

inline constexpr Rectangle kUiPanel = {10, 10, 320, 640};

// Panel de debug para tunear el medio en vivo.
void drawUi(World& world, ToolState& tools);

// Pantalla de inicio: elegir la especie fundadora (dibuja un frame completo).
std::optional<LocomotionType> drawTitleScreen(const Progress& progress);
// Objetivo de la partida y estado del linaje.
void drawRunHud(const Run& run);
// Fin de partida: estadísticas y desbloqueos.
void drawGameOver(const Run& run, const Progress& progress, const std::vector<LocomotionType>& unlocked);
