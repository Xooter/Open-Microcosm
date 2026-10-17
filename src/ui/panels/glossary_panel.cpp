// Glosario (fase P1, paso 14): los términos de los paquetes (glosario.toml) con su definición, su
// fuente, los relacionados y dónde verlos en la simulación (una lección o una placa preparada): la
// "animación" del término es la placa misma.
#include "app/app.hpp"
#include "game/glossary.hpp"
#include "game/lesson.hpp"
#include "game/prepared.hpp"
#include "ui/glossary_ui.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

class GlossaryPanel : public Panel {
  public:
    GlossaryPanel() { open = false; } // se abre desde Ver o con un término de la ficha
    const char* key() const override { return "glosario"; }
    const char* title() const override { return "Glosario"; }
    Dock dock() const override { return Dock::Float; }

    void draw(App& app) override {
        const int n = glossaryCount();
        if (n == 0) {
            note("No hay glosario cargado.");
            return;
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##buscar", tr("Buscar"), filter_, sizeof filter_);
        const std::string wanted = foldCase(filter_);
        // Orden alfabético en el idioma de la interfaz.
        std::vector<int> order;
        for (int i = 0; i < n; ++i) {
            const GlossaryTerm& g = glossaryTerm(i);
            if (!wanted.empty() && foldCase(tr(g.term.c_str())).find(wanted) == std::string::npos &&
                foldCase(tr(g.definition.c_str())).find(wanted) == std::string::npos)
                continue;
            order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [](int a, int b) {
            return foldCase(tr(glossaryTerm(a).term.c_str())) < foldCase(tr(glossaryTerm(b).term.c_str()));
        });
        const float listW = std::min(220.0f * uiScale(), 0.4f * ImGui::GetContentRegionAvail().x);
        ImGui::BeginChild("##terminos", {listW, 0.0f}, ImGuiChildFlags_Borders);
        for (const int i : order) {
            const bool selected = i == app.glossaryIndex;
            if (ImGui::Selectable(tr(glossaryTerm(i).term.c_str()), selected))
                app.glossaryIndex = i;
            if (selected && scrollTo_ != i) {
                ImGui::SetScrollHereY();
                scrollTo_ = i;
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##definicion");
        if (app.glossaryIndex >= 0 && app.glossaryIndex < n)
            drawTerm(app, glossaryTerm(app.glossaryIndex));
        else
            note("Elegí un término. En la ficha y en las lecciones, los términos del glosario aparecen como botones "
                 "debajo del texto.");
        ImGui::EndChild();
    }

  private:
    char filter_[64] = "";
    int scrollTo_ = -1;

    void drawTerm(App& app, const GlossaryTerm& g) {
        ImGui::TextUnformatted(tr(g.term.c_str()));
        ImGui::Separator();
        ImGui::TextWrapped("%s", tr(g.definition.c_str()));
        if (!g.source.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", tr("Fuente:"));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", g.source.c_str());
            ImGui::PopTextWrapPos();
        }
        if (!g.related.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", tr("Relacionados:"));
            for (const std::string& r : g.related) {
                const int k = findGlossaryTerm(r);
                if (k < 0)
                    continue;
                ImGui::SameLine();
                ImGui::PushID(k);
                if (ImGui::SmallButton(tr(glossaryTerm(k).term.c_str())))
                    app.glossaryIndex = k;
                if (ImGui::IsItemHovered())
                    glossaryTooltip(k);
                ImGui::PopID();
            }
        }
        ImGui::Spacing();
        if (const Lesson* l = g.lesson.empty() ? nullptr : findLesson(g.lesson)) {
            if (ImGui::Button((std::string(tr("Verlo en la lección")) + ": " + tr(l->title.c_str())).c_str())) {
                app.lessonRequest = l->key;
                app.panels.show("leccion");
            }
        }
        if (const PreparedPlate* pp = g.plate.empty() ? nullptr : findPreparedPlate(g.plate)) {
            if (ImGui::Button((std::string(tr("Verlo en la placa")) + ": " + tr(pp->name.c_str())).c_str())) {
                PlateSpec plate;
                plate.prepared = pp->key;
                plate.rebuild();
                app.plate = plate;
                app.plateScript.clear();
                app.plateNotes.clear();
                app.createPlate();
            }
            tooltip("Crea esa placa preparada en el centro (reemplaza la que está en pantalla).");
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeGlossaryPanel() { return std::make_unique<GlossaryPanel>(); }

} // namespace ui
