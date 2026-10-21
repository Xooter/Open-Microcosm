// Procesos: las tasas de la química, la microbiología y los hongos, y acciones sobre los solutos.
#include "app/app.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/param_controls.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

namespace {

// Fila de un soluto: concentración media y botones para duplicarla, partirla o vaciarla.
void soluteRow(World& world, const char* name, ChemField& field, float shown, const char* unit) {
    ImGui::PushID(name);
    ImGui::AlignTextToFramePadding();
    const float mean = world.meanConcentration(field);
    ImGui::Text("%s: %.3g %s", tr(name), mean * shown, unit);
    // Pasa por World: lo agregado o quitado entra al balance de masa como entrada externa.
    auto scale = [&](float k) { world.setMeanConcentration(field, k * mean); };
    const float w = 44.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - 3.0f * (w + 6.0f) - 12.0f);
    if (ImGui::Button("x2", {w, 0}))
        scale(2.0f);
    ImGui::SameLine();
    if (ImGui::Button("/2", {w, 0}))
        scale(0.5f);
    ImGui::SameLine();
    if (ImGui::Button("0", {w, 0}))
        scale(0.0f);
    ImGui::PopID();
}

class ProcessesPanel : public Panel {
  public:
    ProcessesPanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "processes"; }
    const char* title() const override { return "Procesos"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        World& world = *app.world;
        const Census& census = app.session.census();
        if (section("Solutos (acciones)", true)) {
            soluteRow(world, "Fosfato", world.phosphateField(), 1.0f, "µM");
            soluteRow(world, "Nitrógeno", world.nitrogenField(), 1.0f, "µM");
            soluteRow(world, "Sílice (Si)", world.silicateField(), 1.0f, "µM");
            soluteRow(world, "Materia orgánica", world.nutrients(), 1.0f, "µM");
            soluteRow(world, "Hierro (Fe)", world.ironField(), 1.0f, "µM");
            soluteRow(world, "Vitamina B12", world.vitaminField(), 1e6f, "pM");
            note("x2, /2 y 0 escalan la concentración de toda la placa. Las concentraciones se fijan en Placa.");
        }
        if (section("Química")) {
            parameterControls(app, ParamGroup::Chemistry);
            readout("O2 de saturación", "%.0f µM", world.oxygenSaturation());
            readout("pH medio", "%.2f (máximo %.2f)", census.meanPH, census.maxPH);
        }
        if (section("Micronutrientes")) {
            parameterControls(app, ParamGroup::Micronutrients);
            note("Si: diatomeas | Fe: fotosíntesis | B12: Euglena y diatomeas");
        }
        if (section("Microbiología"))
            parameterControls(app, ParamGroup::Microbiology);
        if (section("Genética")) {
            parameterControls(app, ParamGroup::Genetics);
            if (app.world && app.world->mutationScale() > 1.0f)
                note("Evolución acelerada: la mutación está por encima de la publicada (no realista).");
        }
        if (section("Hongos y mohos")) {
            parameterControls(app, ParamGroup::Fungi);
            ImGui::Separator();
            note("Physarum (moho mucilaginoso, no es un hongo)");
            parameterControls(app, ParamGroup::Plasmodium);
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeProcessesPanel() { return std::make_unique<ProcessesPanel>(); }

} // namespace ui
