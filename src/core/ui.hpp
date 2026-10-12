#pragma once

#include "render/field_overlay.hpp"

#include <raylib.h>

class World;

struct ToolState {
    float stirRadius = 30.0f; // [µm]
    float inkRadius = 15.0f;  // [µm]
    bool paused = false;
    Overlay overlay = Overlay::None;
};

inline constexpr Rectangle kUiPanel = {10, 10, 300, 330};

// Panel de debug para tunear el medio en vivo.
void drawUi(World& world, ToolState& tools);
