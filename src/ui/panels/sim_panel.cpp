// Simulación: tiempo, velocidad y estado del motor. Las opciones de rendimiento están en
// Preferencias; los parámetros del ambiente, en Placa; los de los procesos, en Procesos.
#include "app/app.hpp"
#include "game/program.hpp"
#include "sim/substep_solver.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/param_controls.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>

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
        if (section("Programa"))
            drawProgram(app, world);
        if (section("Agua y cuerpos blandos (avanzado)"))
            parameterControls(app, ParamGroup::Mechanics);
    }

  private:
    // Programa temporal de la placa (game/program) con las horas biológicas. Cada cambio es una
    // intervención: la escena lo repite.
    static void add(App& app, const ProgramEntry& e) {
        Program p;
        p.entries.push_back(e);
        std::string line = serializeProgram(p).substr(9); // sin "programa="
        line.pop_back();                                  // sin el salto de línea
        std::replace(line.begin(), line.end(), ' ', ',');
        Intervention iv;
        iv.kind = InterventionKind::Program;
        iv.key = line;
        app.intervene(iv);
    }

    static void drawProgram(App& app, const World& world) {
        static const char* const kShapes[] = {"Rampa", "Ciclo", "Seno", "Evento"};
        for (const ProgramEntry& e : world.program().entries)
            ImGui::BulletText("%s: %s, %s %.1f h, %.3g→%.3g, %s %.3g h", e.target.c_str(), tr(kShapes[static_cast<int>(e.shape)]),
                              tr("desde"), e.start, e.from, e.to, tr("período"), e.period);
        if (world.program().empty())
            note("Sin programa: el ambiente queda como se fija a mano.");
        if (ImGui::SmallButton(tr("Fotoperíodo 16:8")))
            add(app, {"luz", ProgramShape::Cycle, 0.0, kProgramForever, 0.0f, 1.0f, 24.0f, 16.0f / 24.0f, 0.0f});
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Ciclo térmico día/noche")))
            add(app, {"temperatura", ProgramShape::Sine, 0.0, kProgramForever, world.medium().temperature, 4.0f, 24.0f, 0.5f, 0.0f});
        tooltip("La temperatura oscila ±4 °C alrededor de la actual cada 24 h biológicas.");
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Pulso de fosfato diario")))
            add(app, {"agregar:fosfato", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 10.0f});
        tooltip("Cada 24 h biológicas se agregan 10 µM de fosfato (un aporte de nutrientes).");
        if (!world.program().empty() && ImGui::SmallButton(tr("Vaciar el programa"))) {
            Intervention iv;
            iv.kind = InterventionKind::Program;
            iv.index = 1;
            app.intervene(iv);
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeSimulationPanel() { return std::make_unique<SimulationPanel>(); }

} // namespace ui
