#include "core/ui.hpp"

#include "sim/world.hpp"

#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

UiActions drawUi(World& world, bool& paused) {
    UiActions actions;
    Rules& r = world.rules();

    GuiPanel({10, 10, 260, 250}, "Simulacion");
    float y = 45;
    auto slider = [&](const char* label, float* value, float lo, float hi) {
        GuiSliderBar({90, y, 120, 18}, label, TextFormat("%.2f", *value), value, lo, hi);
        y += 26;
    };
    slider("rMax", &r.rMax, 10.0f, 120.0f);
    slider("beta", &r.beta, 0.05f, 0.9f);
    slider("fuerza", &r.forceScale, 0.0f, 40.0f);
    slider("friccion", &r.frictionHalfLife, 0.005f, 0.3f);

    actions.randomizeRules = GuiButton({20, y, 115, 24}, "Nuevas reglas");
    actions.respawn = GuiButton({145, y, 115, 24}, "Respawn");
    y += 32;
    GuiCheckBox({20, y, 18, 18}, "Pausa [Espacio]", &paused);
    y += 28;
    GuiLabel({20, y, 240, 18},
             TextFormat("%zu particulas  |  %d FPS", world.particles().size(), GetFPS()));
    return actions;
}
