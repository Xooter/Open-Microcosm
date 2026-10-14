#pragma once

#include "game/sandbox.hpp"

#include <raylib.h>

namespace ui {

// Íconos del catálogo: se dibujan una vez con raylib en una textura (atlas) y los paneles los
// muestran con ImGui. Hay que llamarla después de crear la ventana.
void buildBeingIcons();
void freeBeingIcons();

struct IconUv {
    unsigned int texture = 0;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};
IconUv beingIcon(int index);

} // namespace ui
