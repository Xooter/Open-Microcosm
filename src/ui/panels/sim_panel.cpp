// Simulación: tiempo, velocidad y estado del motor. Las opciones de rendimiento están en
// Preferencias; los parámetros del ambiente, en Placa; los de los procesos, en Procesos.
#include "app/app.hpp"
#include "sim/substep_solver.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/param_controls.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

namespace ui {

namespace {

class SimulationPanel : public Panel {
  public:
    const char* key() const override { return "simulacion"; }
    const char* title() const override { return "Simulación"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        World& world = *app.world;
        ToolState& t = app.tools;
        if (ImGui::Button(tr(t.paused ? "Reanudar" : "Pausar"), {90.0f, 0.0f}))
            t.paused = !t.paused;
        ImGui::SameLine();
        if (ImGui::Button(tr("Un paso"))) {
            t.paused = true;
            ++t.stepRequests;
        }
        sliderLog("Velocidad (x)", &t.simSpeed, 0.125f, 8.0f, "%.2f", "Múltiplo del tiempo real. El paso de la física es fijo.");
        if (section("Tiempo", true)) {
            parameterControls(app, ParamGroup::Time);
            readout("Tiempo simulado", "%.1f s (%.2f h bio)", world.simTime(), world.simTime() * world.bioScale() / 3600.0f);
        }
        if (section("Rendimiento y motor", true)) {
            readout("Rendimiento", "%d FPS | sim %.1f ms | render %.1f ms", GetFPS(), t.stepMs, t.renderMs);
            readout("Motor XPBD", "%s | %zu en detalle", world.substepSolver() ? world.substepSolver()->name() : "CPU",
                    world.nearBodies());
            if (ImGui::SmallButton(tr("Opciones de rendimiento e imagen...")))
                app.panels.show("preferencias");
        }
        if (section("Agua y cuerpos blandos (avanzado)"))
            parameterControls(app, ParamGroup::Mechanics);
    }
};

} // namespace

std::unique_ptr<Panel> makeSimulationPanel() { return std::make_unique<SimulationPanel>(); }

} // namespace ui
