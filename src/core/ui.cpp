#include "core/ui.hpp"

#include "game/world.hpp"

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

void drawUi(World& world, ToolState& tools) {
    MediumParams& m = world.medium();

    GuiPanel(kUiPanel, "Medio (fase 1)");
    float y = kUiPanel.y + 35;
    auto slider = [&](const char* label, float* value, float lo, float hi, const char* fmt) {
        GuiSliderBar({kUiPanel.x + 120, y, 110, 16}, label, TextFormat(fmt, *value), value, lo,
                     hi);
        y += 22;
    };
    slider("espesor [um]", &world.fluid().gapHeight, 20.0f, 500.0f, "%.0f");
    slider("radio revolver", &tools.stirRadius, 5.0f, 120.0f, "%.0f");
    slider("radio tinta", &tools.inkRadius, 2.0f, 60.0f, "%.0f");
    slider("quimiotaxis", &world.bacteria().params.chemotaxisGain, 0.0f, 20.0f, "%.1f");
    slider("descomposicion", &m.detritusDecay, 0.0f, 0.02f, "%.4f");
    slider("consumo nutr.", &m.nutrientUptake, 0.0f, 0.05f, "%.3f");
    slider("detritos/s", &m.detritusSpawnRate, 0.0f, 5.0f, "%.1f");

    GuiCheckBox({kUiPanel.x + 10, y + 2, 16, 16}, "Pausa [Espacio]", &tools.paused);
    y += 26;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("t = %.1f s  |  %d FPS", world.simTime(), GetFPS()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("%zu bacterias  |  %zu particulas", world.bacteria().size(),
                        world.ambient().size()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("Overlay: %s", overlayName(tools.overlay)));
    y += 22;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "Izq: revolver | T: tinta | D: detrito");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "F1-F4: overlays | Der: mover | Rueda: zoom");
}
