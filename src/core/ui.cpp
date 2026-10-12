#include "core/ui.hpp"

#include "game/world.hpp"

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

void drawUi(World& world, ToolState& tools) {
    MediumParams& m = world.medium();

    GuiPanel(kUiPanel, "Medio y celulas (fase 2)");
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

    SoftBodyTuning& t = world.softTuning();
    float substeps = static_cast<float>(t.substeps);
    slider("subpasos", &substeps, 1.0f, 30.0f, "%.0f");
    t.substeps = static_cast<int>(substeps + 0.5f);
    slider("memoria forma x", &t.shapeScale, 0.0f, 3.0f, "%.2f");
    slider("flexion [1/s]", &t.bendRate, 0.0f, 400.0f, "%.0f");
    slider("area [1/s]", &t.areaRate, 10.0f, 3000.0f, "%.0f");
    slider("apretar [um/s]", &tools.squeezeSpeed, 5.0f, 120.0f, "%.0f");

    GuiCheckBox({kUiPanel.x + 10, y + 2, 16, 16}, "Pausa [Espacio]", &tools.paused);
    y += 26;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("t = %.1f s  |  %d FPS", world.simTime(), GetFPS()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("%zu celulas | %zu bacterias | %zu part.", world.cellCount(),
                        world.bacteria().size(), world.ambient().size()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("Overlay: %s", overlayName(tools.overlay)));
    y += 22;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "Izq: revolver | T: tinta | D: detrito");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "F1-F4: overlays | Der: mover | Rueda: zoom");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "C: celula | K: 50 celulas | X: borrar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 280, 16}, "Shift+Izq: estirar | Q: apretar al cursor");
}
