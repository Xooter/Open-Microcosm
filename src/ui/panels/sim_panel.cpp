// Simulación: tiempo, velocidad, rendimiento y motor. Los parámetros del ambiente están en
// Placa; los de los procesos, en Procesos.
#include "app/app.hpp"
#include "sim/substep_solver.hpp"
#include "ui/panels/panel_classes.hpp"
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
            if (slider("Tiempo biológico (x)", &world.bioTimeScale(), 1.0f, 600.0f, "%.0f",
                       "Compresión de la biología: 60 = 1 min real es 1 h biológica. La física siempre va en tiempo real."))
                app.options.bioTimeScale = world.bioTimeScale();
            readout("Tiempo simulado", "%.1f s (%.2f h bio)", world.simTime(), world.simTime() * world.bioScale() / 3600.0f);
        }
        if (section("Rendimiento y motor", true)) {
            bool lod = app.options.lod;
            if (checkbox("LOD: simplificar lo lejano", &lod, "Lo que está fuera de la vista se simula con menos detalle.")) {
                app.options.lod = lod;
                app.applyOptions();
            }
            bool gpu = app.options.gpuPhysics;
            ImGui::BeginDisabled(!app.gpuOk);
            if (checkbox("Física de células en la GPU", &gpu, "Compute shaders (OpenGL 4.3).")) {
                app.options.gpuPhysics = gpu;
                app.applyOptions();
            }
            ImGui::EndDisabled();
            bool post = app.options.postProcess;
            if (checkbox("Óptica del microscopio", &post, "Viñeta, aberración cromática y grano.")) {
                app.options.postProcess = post;
                app.applyOptions();
            }
            readout("Rendimiento", "%d FPS | sim %.1f ms | render %.1f ms", GetFPS(), t.stepMs, t.renderMs);
            readout("Motor XPBD", "%s | %zu en detalle", world.substepSolver() ? world.substepSolver()->name() : "CPU",
                    world.nearBodies());
        }
        if (section("Agua y cuerpos blandos (avanzado)")) {
            slider("Espesor de película [µm]", &world.fluid().gapHeight, 20.0f, 500.0f, "%.0f",
                   "Controla qué tan locales son las corrientes.");
            SoftBodyTuning& t2 = world.softTuning();
            float substeps = static_cast<float>(t2.substeps);
            if (slider("Subpasos XPBD", &substeps, 1.0f, 30.0f, "%.0f"))
                t2.substeps = static_cast<int>(substeps + 0.5f);
            slider("Memoria de forma (x)", &t2.shapeScale, 0.0f, 3.0f, "%.2f");
            slider("Flexión [1/s]", &t2.bendRate, 0.0f, 400.0f, "%.0f");
            slider("Área [1/s]", &t2.areaRate, 10.0f, 3000.0f, "%.0f");
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeSimulationPanel() { return std::make_unique<SimulationPanel>(); }

} // namespace ui
