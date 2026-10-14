#pragma once

#include "app/tools.hpp"

#include <imgui.h>

// Ícono vectorial de una herramienta, dibujado con la lista de dibujo de ImGui (sin texturas),
// centrado en `c` y de ~`size` px de lado.
void drawToolIcon(ImDrawList* dl, Tool tool, ImVec2 c, float size, ImU32 color);
