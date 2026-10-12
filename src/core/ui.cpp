#include "core/ui.hpp"

#include "bio/division.hpp"
#include "game/world.hpp"

#include <cmath>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

void drawUi(World& world, ToolState& tools) {
    MediumParams& m = world.medium();

    GuiPanel(kUiPanel, "BioGame (fase 3)");
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
    y += 4;
    const entt::registry& reg = world.registry();
    if (reg.valid(world.player())) {
        const entt::entity p = world.player();
        const Locomotion& loco = reg.get<Locomotion>(p);
        const Digestion& dig = reg.get<Digestion>(p);
        const Metabolism& meta = reg.get<Metabolism>(p);
        const Dividing* div = reg.try_get<Dividing>(p);
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("Jugador: %s  |  %.0f um/s  |  %.1f h", locomotionName(loco.type),
                            std::abs(loco.speed), meta.ageHours));
        y += 20;
        // Barras de estado: reservas, crecimiento hacia la división y progreso de la división.
        float reserves = meta.reserves / std::max(1e-6f, meta.biomass);
        GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "reservas", TextFormat("%.0f%%", 100 * reserves),
                       &reserves, 0.0f, 0.6f);
        y += 18;
        float growth = meta.biomass / std::max(1e-6f, meta.birthBiomass);
        GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "biomasa", TextFormat("x%.2f", growth), &growth,
                       0.5f, 2.0f);
        y += 18;
        if (div) {
            float pr = std::min(1.0f, div->progress);
            GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "division", TextFormat("%.0f%%", 100 * pr), &pr,
                           0.0f, 1.0f);
            y += 18;
        }
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("O2 %.0f uM (%.0f%% aerobico) | luz %.2f%s", meta.o2, 100 * meta.aerobic,
                            meta.light, meta.starving ? " | AYUNO" : ""));
        y += 18;
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("Comido: %d bact. %d detr. %d cel. | %zu vacuolas", dig.bacteriaEaten,
                            dig.detritusEaten, dig.cellsEaten, dig.vacuoles.size()));
        y += 22;
    } else {
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("Tu celula murio (%s). R: reaparecer", world.lastDeathCause()));
        y += 22;
    }
    slider("luz", &world.medium().surfaceLight, 0.0f, 1.0f, "%.2f");
    slider("tiempo bio x", &world.bioTimeScale(), 1.0f, 600.0f, "%.0f");
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "1 cilios | 2 flagelo | 3 ameba | F seguir");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Cilios: W/S avanzar, A/D girar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Flagelo: W o click, gira hacia el mouse");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Ameba: click = pseudopodo; toca y envuelve");
    y += 20;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Ctrl+Izq revolver | Shift+Izq estirar | T tinta");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "B detrito | C celula | K 50 celulas | X borrar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Q apretar | L lisar | F1-F4 overlays | Rueda");
}
