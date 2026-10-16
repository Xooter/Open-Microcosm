// Gráficos en vivo (fase 13): las series que muestrea la sesión, con zoom (rueda y arrastre sobre
// el gráfico), y la curva de crecimiento ajustada de una población.
#include "app/app.hpp"
#include "game/growth_fit.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ui {

namespace {

// Pestañas: grupo de columnas de la serie que muestran.
struct Tab {
    const char* title;
    const char* group;
};
constexpr Tab kTabs[] = {
    {"Poblaciones", "Poblacion"},
    {"Química", "Quimica"},
    {"Balance", "Balance"},
    {"Región", "Region"},
};

// ¿La columna tiene algún dato distinto de cero (las especies ausentes no se ofrecen)?
bool hasData(const std::vector<float>& v) {
    for (float x : v)
        if (x != 0.0f && finiteNumber(x))
            return true;
    return false;
}

class GraphsPanel : public Panel {
  public:
    GraphsPanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "graficos"; }
    const char* title() const override { return "Gráficos"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        const TimeSeries& s = app.session.series();
        const int tBio = s.find("t bio");
        if (s.rows() == 0 || tBio < 0) {
            note("Todavía no hay muestras: la sesión toma una por intervalo de muestreo.");
            return;
        }
        syncSelection(s);
        float interval = app.session.sampleInterval();
        if (sliderLog("Muestreo [s]", &interval, 0.25f, 30.0f, "%.2f",
                      "Cada cuánto (en segundos de simulación) se toma una muestra. Con muchas muestras la serie se "
                      "decima: guarda una de cada dos."))
            app.session.setSampleInterval(interval);
        checkbox("Ajustar ejes", &autoFit_, "Los ejes siguen a los datos. Apagado: zoom libre con la rueda y arrastre.");
        ImGui::SameLine();
        if (ImGui::Button(tr("Exportar CSV/JSON")))
            app.exportData();
        tooltip("Guarda la serie (CSV), la corrida con sus parámetros y el balance (JSON) y las fichas de especies.");
        ImGui::TextDisabled("%zu %s, %s %d", s.rows(), tr("muestras"), tr("decimación x"), s.stride());

        const std::vector<float>& x = s.column(static_cast<std::size_t>(tBio));
        if (ImGui::BeginTabBar("##graphs")) {
            for (const Tab& tab : kTabs)
                if (ImGui::BeginTabItem(tr(tab.title))) {
                    drawGroup(s, x, tab.group);
                    ImGui::EndTabItem();
                }
            if (ImGui::BeginTabItem(tr("Crecimiento"))) {
                drawGrowth(s, x);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

  private:
    std::vector<std::uint8_t> shown_;
    std::size_t columns_ = 0;
    bool autoFit_ = true;
    bool logScale_ = false;
    int growthColumn_ = -1;
    // Caché del ajuste (se rehace cuando llega una muestra nueva o cambia la población).
    std::size_t fitRows_ = 0;
    int fitColumn_ = -2;
    GrowthFit fit_;
    std::vector<float> curveT_, curveN_, tangentT_, tangentN_;

    // El gráfico llena lo que queda del panel, con un mínimo legible (si no entra, el panel se desplaza).
    static float plotHeight() { return std::max(220.0f, ImGui::GetContentRegionAvail().y); }

    void syncSelection(const TimeSeries& s) {
        if (columns_ == s.columns())
            return;
        columns_ = s.columns();
        shown_.assign(columns_, 0);
        // Por defecto: O₂, el balance de P y la región.
        for (const char* name : {"O2", "P total", "P disuelto", "P en seres", "Región: células"})
            if (const int c = s.find(name); c >= 0)
                shown_[static_cast<std::size_t>(c)] = 1;
        growthColumn_ = -1;
    }

    void drawGroup(const TimeSeries& s, const std::vector<float>& x, const char* group) {
        const bool populations = std::strcmp(group, "Poblacion") == 0;
        // Selector de series a la izquierda del gráfico; si el panel es angosto, arriba.
        const bool narrow = ImGui::GetContentRegionAvail().x < 560.0f;
        ImGui::BeginChild("##pick", narrow ? ImVec2{0.0f, 120.0f} : ImVec2{200.0f, 0.0f},
                          ImGuiChildFlags_Borders | (narrow ? ImGuiChildFlags_ResizeY : ImGuiChildFlags_ResizeX));
        if (populations)
            checkbox("Escala log", &logScale_);
        int offered = 0;
        for (std::size_t c = 0; c < s.columns(); ++c) {
            if (s.info(c).group != group || (populations && !hasData(s.column(c))))
                continue;
            ++offered;
            bool on = shown_[c] != 0;
            const std::string label = s.info(c).unit.empty() ? std::string(tr(s.info(c).name.c_str()))
                                                             : std::string(tr(s.info(c).name.c_str())) + " [" + s.info(c).unit + "]";
            if (ImGui::Checkbox(label.c_str(), &on))
                shown_[c] = on ? 1 : 0;
        }
        if (offered == 0)
            note(std::strcmp(group, "Region") == 0 ? "Dibujá una región con la herramienta Región (G)." : "Sin datos todavía.");
        ImGui::EndChild();
        if (!narrow)
            ImGui::SameLine();

        const ImPlotAxisFlags fit = autoFit_ ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None;
        if (ImPlot::BeginPlot((std::string("##plot") + group).c_str(), {-1.0f, plotHeight()})) {
            ImPlot::SetupAxes(tr("tiempo biológico [h]"), nullptr, fit, fit);
            // El pH no tiene unidad ni la escala de las concentraciones: va en un eje propio.
            const bool chemistry = std::strcmp(group, "Quimica") == 0;
            const int phColumn = s.find("pH");
            if (chemistry && phColumn >= 0 && shown_[static_cast<std::size_t>(phColumn)])
                ImPlot::SetupAxis(ImAxis_Y2, "pH", ImPlotAxisFlags_AuxDefault | fit);
            if (populations && logScale_)
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlotSpec spec;
            spec.Flags = ImPlotLineFlags_SkipNaN;
            spec.LineWeight = 1.5f;
            for (std::size_t c = 0; c < s.columns(); ++c) {
                if (!shown_[c] || s.info(c).group != group)
                    continue;
                const bool ph = chemistry && static_cast<int>(c) == phColumn;
                ImPlot::SetAxes(ImAxis_X1, ph ? ImAxis_Y2 : ImAxis_Y1);
                ImPlot::PlotLine(tr(s.info(c).name.c_str()), x.data(), s.column(c).data(), static_cast<int>(x.size()), spec);
            }
            ImPlot::EndPlot();
        }
    }

    void drawGrowth(const TimeSeries& s, const std::vector<float>& x) {
        // Poblaciones con datos (especies, bacterias, cianobacterias).
        std::vector<int> cols;
        std::vector<const char*> names;
        for (std::size_t c = 0; c < s.columns(); ++c)
            if (s.info(c).group == "Poblacion" && s.info(c).unit != "pmol" && hasData(s.column(c))) {
                cols.push_back(static_cast<int>(c));
                names.push_back(s.info(c).name.c_str());
            }
        if (cols.empty()) {
            note("No hay poblaciones en la placa.");
            return;
        }
        int idx = static_cast<int>(std::find(cols.begin(), cols.end(), growthColumn_) - cols.begin());
        if (idx >= static_cast<int>(cols.size()))
            idx = 0;
        if (combo("Población", &idx, names.data(), static_cast<int>(names.size()),
                  "Ajusta la curva de crecimiento: µmax, tiempo de duplicación, latencia (Pirt) y capacidad de carga "
                  "(logística de Verhulst)."))
            fitRows_ = 0;
        growthColumn_ = cols[static_cast<std::size_t>(idx)];
        const std::vector<float>& n = s.column(static_cast<std::size_t>(growthColumn_));
        refit(x, n);

        if (fit_.valid) {
            readout("µmax", "%.3f /h", fit_.muMax);
            readout("Tiempo de duplicación", "%.2f h", fit_.doublingHours);
            readout("Latencia", "%.2f h", fit_.lagHours);
            if (fit_.logisticValid)
                readout("Capacidad de carga K", "%.0f %s", fit_.capacity,
                        fit_.plateau ? tr("(meseta alcanzada)") : tr("(estimada: todavía crece)"));
        } else {
            note("Sin crecimiento medible todavía (hacen falta al menos 5 muestras con la población en aumento).");
        }
        if (ImPlot::BeginPlot("##growth", {-1.0f, plotHeight()})) {
            const ImPlotAxisFlags fit = autoFit_ ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None;
            ImPlot::SetupAxes(tr("tiempo biológico [h]"), tr("individuos"), fit, fit);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlotSpec data;
            data.Marker = ImPlotMarker_Circle;
            data.MarkerSize = 2.5f;
            data.Flags = ImPlotLineFlags_SkipNaN;
            ImPlot::PlotLine(names[static_cast<std::size_t>(idx)], x.data(), n.data(), static_cast<int>(x.size()), data);
            ImPlotSpec model;
            model.LineWeight = 2.0f;
            if (fit_.logisticValid && !curveT_.empty())
                ImPlot::PlotLine(tr("Logística"), curveT_.data(), curveN_.data(), static_cast<int>(curveT_.size()), model);
            if (fit_.valid && !tangentT_.empty())
                ImPlot::PlotLine(tr("Fase exponencial"), tangentT_.data(), tangentN_.data(), static_cast<int>(tangentT_.size()), model);
            if (fit_.valid) {
                const double lag = x.front() + fit_.lagHours;
                ImPlot::PlotInfLines(tr("Latencia"), &lag, 1);
            }
            if (fit_.logisticValid) {
                const double k = fit_.capacity;
                ImPlotSpec h;
                h.Flags = ImPlotInfLinesFlags_Horizontal;
                ImPlot::PlotInfLines("K", &k, 1, h);
            }
            ImPlot::EndPlot();
        }
    }

    void refit(const std::vector<float>& x, const std::vector<float>& n) {
        if (fitRows_ == x.size() && fitColumn_ == growthColumn_)
            return; // cada muestra nueva cambia la cantidad de filas (también al decimar)
        fitRows_ = x.size();
        fitColumn_ = growthColumn_;
        fit_ = fitGrowth(x.data(), n.data(), x.size());
        curveT_.clear();
        curveN_.clear();
        tangentT_.clear();
        tangentN_.clear();
        if (x.size() < 2)
            return;
        const float t0 = x.front(), t1 = x.back();
        if (fit_.logisticValid)
            for (int i = 0; i <= 120; ++i) {
                const float t = t0 + (t1 - t0) * static_cast<float>(i) / 120.0f;
                curveT_.push_back(t);
                curveN_.push_back(fit_.logistic(t));
            }
        if (fit_.valid) {
            // Tangente de la fase exponencial: desde la latencia hasta el máximo observado.
            float nMax = 0.0f;
            for (float v : n)
                nMax = std::max(nMax, v);
            const float ta = t0 + fit_.lagHours;
            const float tb = std::min(t1, (std::log(std::max(nMax, 1.0f)) - fit_.tangentIntercept) / fit_.muMax);
            for (int i = 0; i <= 20 && tb > ta; ++i) {
                const float t = ta + (tb - ta) * static_cast<float>(i) / 20.0f;
                tangentT_.push_back(t);
                tangentN_.push_back(std::exp(fit_.tangentIntercept + fit_.muMax * t));
            }
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeGraphsPanel() { return std::make_unique<GraphsPanel>(); }

} // namespace ui
