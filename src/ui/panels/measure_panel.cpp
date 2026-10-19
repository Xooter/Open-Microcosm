// Medición (fase 13): regla y escala, cuantificación por región, mapa de limitación y exportación.
#include "app/app.hpp"
#include "game/region.hpp"
#include "render/cell_renderer.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <cstdint>

namespace ui {

namespace {

class MeasurePanel : public Panel {
  public:
    const char* key() const override { return "measurement"; }
    const char* title() const override { return "Medición"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        ToolState& t = app.tools;
        if (section("Regla y escala", true))
            drawRuler(app, t);
        if (section("Región", true))
            drawRegion(app);
        if (section("Mapa de limitación", true))
            drawLimitation(app, t);
        if (section("Exportar datos", true)) {
            if (ImGui::Button(tr("Exportar CSV/JSON")))
                app.exportData();
            tooltip("Serie temporal con unidades (CSV), la corrida con sus parámetros y su balance de masa (JSON) y "
                    "las fichas de especies (JSON). Ctrl+E.");
            note(app.exportsDir.c_str());
            if (!app.lastExport.empty())
                readout("Última", "%s", app.lastExport.c_str());
        }
        if (section("Foto y time-lapse", true))
            drawPhoto(app);
    }

  private:
    void drawPhoto(App& app) {
        if (ImGui::Button(tr("Foto del campo")))
            app.requestPhoto();
        tooltip("Lo que se ve en el centro (sin la interfaz), con una barra de escala y el tiempo biológico, en PNG; al "
                "lado, un JSON con el ancho del campo en µm, el tiempo, la seed, la placa, la tinción y la versión. No "
                "hay objetivos simulados: se da el ancho real del campo, no un aumento. F12.");
        if (!app.lastPhoto.empty())
            readout("Última", "%s", app.lastPhoto.c_str());
        bool on = app.timelapse;
        if (checkbox("Time-lapse", &on, "Una foto cada tanto tiempo BIOLÓGICO de la placa (no por cuadro): la serie no "
                                         "depende de los FPS ni de la velocidad. Cada time-lapse va a una carpeta nueva."))
            app.setTimelapse(on);
        float minutes = static_cast<float>(app.timelapseClock.intervalHours * 60.0);
        if (sliderLog("Intervalo", &minutes, 1.0f, 600.0f, "%.0f min bio", "Tiempo biológico entre fotos."))
            app.timelapseClock.intervalHours = minutes / 60.0;
        if (app.timelapse) {
            readout("Cuadros", "%d", app.timelapseClock.frame);
            note(app.timelapseDir.c_str());
        } else {
            note(app.photosDir.c_str());
        }
    }

    void drawRuler(App& app, ToolState& t) {
        static const char* const kModes[] = {"Distancia", "Área"};
        int mode = static_cast<int>(t.measureMode);
        if (combo("Modo", &mode, kModes, 2, "Distancia: arrastrá con la herramienta Medir. Área: dibujá un lazo."))
            t.measureMode = static_cast<ToolState::MeasureMode>(mode);
        if (t.tool != Tool::Measure && ImGui::SmallButton(tr("Usar la herramienta Medir (M)")))
            app.setTool(Tool::Measure);
        if (t.measurePath.size() >= 2) {
            if (t.measureMode == ToolState::MeasureMode::Distance) {
                readout("Distancia", "%.1f µm", polylineLength(t.measurePath));
            } else {
                readout("Área", "%.0f µm² (%.4f mm²)", polygonArea(t.measurePath), polygonArea(t.measurePath) * 1e-6f);
                readout("Perímetro", "%.0f µm",
                        polylineLength(t.measurePath) + polylineLength({t.measurePath.back(), t.measurePath.front()}));
            }
            if (ImGui::SmallButton(tr("Borrar medición")))
                t.measurePath.clear();
        }
        checkbox("Barra de escala", &t.scaleBar, "Barra con un largo redondo en µm, abajo a la izquierda de la placa.");
    }

    void drawRegion(App& app) {
        const Run& run = app.session;
        const RegionStats& r = run.regionStats();
        if (run.region().empty() || !r.valid) {
            note("Dibujá un lazo con la herramienta Región (G): se cuentan las células y se mide la química adentro, y "
                 "entra en la serie de Gráficos.");
            if (app.tools.tool != Tool::Region && ImGui::SmallButton(tr("Usar la herramienta Región (G)")))
                app.setTool(Tool::Region);
            return;
        }
        readout("Área", "%.0f µm² (%.4f mm²)", r.area, r.area * 1e-6f);
        readout("Células", "%d (%.0f /mm²)", r.cells, r.cellDensity());
        readout("Bacterias", "%d", r.bacteria);
        readout("Biomasa", "%.1f pmol", r.biomass);
        readout("O2", "%.1f µM", r.oxygen);
        readout("pH", "%.2f", r.ph);
        readout("Fosfato / N inorg.", "%.2f / %.1f µM", r.phosphate, r.nitrogen);
        readout("Sílice / Hierro", "%.1f / %.3f µM", r.silicate, r.iron);
        readout("Vitamina B12", "%.1f pM", r.vitamin * 1e6f);
        readout("Materia orgánica", "%.2f µM", r.organic);
        for (int s = 0; s < speciesCount(); ++s)
            if (r.species[s] > 0)
                readout(speciesInfo(static_cast<SpeciesId>(s)).name, "%d", r.species[s]);
        if (ImGui::SmallButton(tr("Borrar región")))
            app.session.clearRegion();
    }

    void drawLimitation(App& app, ToolState& t) {
        checkbox("Colorear las células", &t.limitationMap,
                 "Cada célula toma el color de lo que limita su crecimiento ahora (ley del mínimo de Liebig) [L].");
        // Cuántas células limita cada factor (recorre solo las células: es barato).
        int counts[static_cast<int>(Limitation::Count)] = {};
        for (auto [e, m] : app.world->registry().view<Metabolism>().each())
            ++counts[static_cast<int>(m.limiting)];
        for (int i = 0; i < static_cast<int>(Limitation::Count); ++i) {
            const Color c = limitationColor(static_cast<Limitation>(i));
            ImGui::ColorButton(("##lim" + std::to_string(i)).c_str(), {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f},
                               ImGuiColorEditFlags_NoTooltip, {12.0f, 12.0f});
            ImGui::SameLine();
            ImGui::Text("%s: %d", tr(limitationName(static_cast<Limitation>(i))), counts[i]);
        }
        note("Energía: a las heterótrofas les falta comida; a las fotótrofas, luz. P y N limitan solo a las fotótrofas; "
             "Si, a las diatomeas; B12, a las auxótrofas.");
    }
};

} // namespace

std::unique_ptr<Panel> makeMeasurePanel() { return std::make_unique<MeasurePanel>(); }

} // namespace ui
