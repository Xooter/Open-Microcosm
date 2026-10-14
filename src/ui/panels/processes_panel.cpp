// Procesos: las tasas de la química, la microbiología y los hongos, y acciones sobre los solutos.
#include "app/app.hpp"
#include "bio/fungi.hpp"
#include "bio/plasmodium.hpp"
#include "core/units.hpp"
#include "ui/panels/panel_classes.hpp"
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
    const char* key() const override { return "procesos"; }
    const char* title() const override { return "Procesos"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        World& world = *app.world;
        MediumParams& m = world.medium();
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
            sliderLog("Alcalinidad [µM]", &m.alkalinity, 50.0f, 5000.0f, "%.0f");
            slider("Descomposición [1/h bio]", &m.detritusDecay, 0.0f, 0.02f, "%.4f");
            slider("Consumo de nutrientes [1/s]", &m.nutrientUptake, 0.0f, 0.05f, "%.3f");
            slider("Detritos que llegan [1/s]", &m.detritusSpawnRate, 0.0f, 5.0f, "%.1f");
            slider("Aporte orgánico [µM/h]", &m.organicInflow, 0.0f, 500.0f, "%.0f");
            slider("Polvo mineral [1/s]", &m.dustRate, 0.0f, 0.2f, "%.3f");
            readout("O2 de saturación", "%.0f µM", world.oxygenSaturation());
            readout("pH medio", "%.2f (máximo %.2f)", census.meanPH, census.maxPH);
        }
        if (section("Micronutrientes")) {
            sliderLog("B12 de las bacterias", &m.vitaminSourcePerHour, 1e-7f, 1e-3f, "%.1e");
            slider("B12 la degrada la luz [1/h]", &m.vitaminDecayPerHour, 0.0f, 0.5f, "%.3f");
            slider("Disolución de frústulas [1/h]", &m.frustuleDissolutionPerHour, 0.0f, 0.05f, "%.4f");
            note("Si: diatomeas | Fe: fotosíntesis | B12: Euglena y diatomeas");
        }
        if (section("Microbiología")) {
            slider("Quimiotaxis", &world.bacteria().params.chemotaxisGain, 0.0f, 20.0f, "%.1f");
            slider("Fagos: inactivación [1/h]", &m.phageDecayPerHour, 0.0f, 1.0f, "%.2f");
            slider("Toxinas: degradación [1/h]", &m.toxinDecayPerHour, 0.0f, 0.5f, "%.2f");
            slider("AHL: vida media [h]", &m.ahlHalfLifeHours, 1.0f, 100.0f, "%.0f");
            slider("Nauplios [1/h]", &m.naupliusArrivalsPerHour, 0.0f, 1.0f, "%.2f");
        }
        if (section("Hongos y mohos")) {
            FungiParams& fp = world.fungi().params;
            slider("Absorción [1/h]", &fp.uptakePerHour, 0.0f, 4.0f, "%.2f");
            slider("Mantenimiento [1/h]", &fp.maintenancePerHour, 0.0f, 0.2f, "%.3f");
            slider("Hambre a esporas [h]", &fp.starvationHoursToSporulate, 0.5f, 20.0f, "%.1f");
            slider("Exoenzimas (x)", &fp.detritusDecayBoost, 1.0f, 100.0f, "%.0f");
            slider("Allomyces: despierta [µM]", &fp.restingGerminateFood, 1.0f, 50.0f, "%.0f");
            slider("Liquen: azúcar [1/h]", &fp.lichenSugarPerHour, 0.0f, 0.5f, "%.2f");
            slider("Liquen: fósforo [1/h]", &fp.lichenPhosphorusPerHour, 0.0f, 0.5f, "%.2f");
            ImGui::Separator();
            note("Physarum (moho mucilaginoso, no es un hongo)");
            PlasmodiumParams& pp = world.plasmodia().params;
            slider("Frente [µm/h bio]", &pp.tipSpeedPerHour, 200.0f, 8000.0f, "%.0f");
            slider("Adaptación de tubos [1/h]", &pp.adaptRate, 0.1f, 4.0f, "%.2f");
            slider("Respuesta al flujo (gamma)", &pp.gamma, 0.5f, 3.0f, "%.2f");
            slider("Cambio fuente/sumidero [h]", &pp.switchHours, 0.02f, 2.0f, "%.2f");
            slider("Evita su baba", &pp.slimeAvoidance, 0.0f, 4.0f, "%.1f");
            slider("Evita la luz", &pp.lightAvoidance, 0.0f, 4.0f, "%.1f");
            slider("Retracción (D mínimo)", &pp.pruneD, 0.0f, 0.2f, "%.3f");
            slider("Hambre a esclerocio [h]", &pp.starveHoursToSclerotium, 1.0f, 40.0f, "%.0f");
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeProcessesPanel() { return std::make_unique<ProcessesPanel>(); }

} // namespace ui
