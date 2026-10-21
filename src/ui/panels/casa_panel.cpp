// CASA: la cinemática de los espermatozoides de la placa, como un equipo de análisis de semen (game/casa).
#include "app/app.hpp"
#include "game/casa.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>

namespace ui {

namespace {

class CasaPanel : public Panel {
  public:
    CasaPanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "casa"; }
    const char* title() const override { return "CASA"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        Casa& casa = app.session.casa();
        if (checkbox("Medir", &casa.enabled,
                     "Sigue la cabeza de cada espermatozoide cuadro a cuadro y mide su cinemática (VCL, VSL, VAP, LIN, "
                     "STR, WOB, ALH, BCF), como un equipo CASA. Solo mide: no cambia la placa.") &&
            !casa.enabled)
            casa.clear();
        if (slider("Trayectoria [s]", &casa.trackSeconds, 0.5f, 3.0f, "%.1f",
                   "Cuánto de la trayectoria de cada uno se analiza (los equipos usan ~0,5–1 s)."))
            casa.clear();
        if (!casa.enabled) {
            note("Activalo y esperá un segundo: cada espermatozoide necesita su trayectoria completa.");
            return;
        }
        const CasaSummary& s = casa.summary();
        readout("Analizados", "%d", s.tracked);
        if (s.tracked == 0)
            return;
        if (section("Clases (OMS 2021)", true)) {
            readout("Progresivos rápidos", "%.0f %%", 100.0 * s.fraction(CasaGrade::Rapid));
            readout("Progresivos lentos", "%.0f %%", 100.0 * s.fraction(CasaGrade::Slow));
            readout("No progresivos", "%.0f %%", 100.0 * s.fraction(CasaGrade::NonProgressive));
            readout("Inmóviles", "%.0f %%", 100.0 * s.fraction(CasaGrade::Immotile));
            note("Inmóviles = vivos que no baten + muertos: para separarlos, eosina–nigrosina o la prueba hiposmótica.");
        }
        if (section("Cinemática media (móviles)", true)) {
            readout("VCL", "%.1f µm/s", static_cast<double>(s.vcl));
            readout("VSL", "%.1f µm/s", static_cast<double>(s.vsl));
            readout("VAP", "%.1f µm/s", static_cast<double>(s.vap));
            readout("LIN", "%.0f %%", 100.0 * s.lin);
            readout("STR", "%.0f %%", 100.0 * s.str);
            readout("WOB", "%.0f %%", 100.0 * s.wob);
            readout("ALH", "%.2f µm", static_cast<double>(s.alh));
            readout("BCF", "%.1f Hz", static_cast<double>(s.bcf));
        }
        if (section("Distribución de la VCL", true)) {
            vcl_.clear();
            for (const CasaKinematics& k : casa.results())
                vcl_.push_back(static_cast<double>(k.vcl));
            if (ImPlot::BeginPlot("##vcl", {-1.0f, 160.0f * uiScale()})) {
                ImPlot::SetupAxes(tr("VCL [µm/s]"), tr("espermatozoides"), ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                ImPlot::PlotHistogram("VCL", vcl_.data(), static_cast<int>(vcl_.size()), 20);
                ImPlot::EndPlot();
            }
        }
        note("La exportación (Ctrl+E) suma la tabla de cada uno (_casa.csv).");
    }

  private:
    std::vector<double> vcl_;
};

} // namespace

std::unique_ptr<Panel> makeCasaPanel() { return std::make_unique<CasaPanel>(); }

} // namespace ui
