// Ficha: qué es lo elegido con la herramienta Seleccionar, qué hace y por qué.
#include "app/app.hpp"
#include "bio/genome.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <cmath>

namespace ui {

namespace {

ImVec4 col(const float (&a)[4]) { return ImVec4(a[0], a[1], a[2], a[3]); }

void drawGenome(const World& world, entt::entity cell) {
    const entt::registry& reg = world.registry();
    if (!reg.valid(cell) || !reg.all_of<Heredity, Locomotion>(cell))
        return;
    const Genome& g = reg.get<Heredity>(cell).genome;
    const LocomotionType type = reg.get<Locomotion>(cell).type;
    for (std::size_t i = 0; i < kGeneCount; ++i) {
        const auto gene = static_cast<Gene>(i);
        if (!geneAllowed(type, gene))
            continue;
        const GeneInfo& info = geneInfo(gene);
        if (gene == Gene::MatingType) {
            readout(info.name, "%s", g.v[i] > 0.5f ? "II" : "I");
        } else {
            char text[24];
            std::snprintf(text, sizeof text, "%.2f", g.v[i]);
            bar(info.name, (g.v[i] - info.min) / (info.max - info.min), text);
        }
    }
}

class InspectorPanel : public Panel {
  public:
    const char* key() const override { return "ficha"; }
    const char* title() const override { return "Ficha"; }
    Dock dock() const override { return Dock::Right; }

    void draw(App& app) override {
        SelectionState& st = app.selection;
        const Inspection& ins = st.inspection;
        if (!ins.valid) {
            note("Con la herramienta Seleccionar (1), hacé click sobre cualquier ser, grano o partícula de la placa para ver qué es, qué hace y por qué.");
            return;
        }
        if (st.followable()) {
            checkbox("Seguir con la cámara", &app.tools.followSelected);
            ImGui::SameLine();
        }
        if (ImGui::SmallButton(tr("Cerrar"))) {
            st.picked = {};
            st.inspection = {};
            app.tools.followSelected = false;
            return;
        }
        ImGui::TextColored(ins.dead ? ImVec4(0.9f, 0.35f, 0.3f, 1.0f) : ImVec4(1, 1, 1, 1), "%s", ins.title.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
        ImGui::TextWrapped("%s", ins.subtitle.c_str());
        ImGui::PopStyleColor();
        if (ins.dead && ins.successor != entt::null)
            note("El control pasa a su sucesor.");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", ins.blurb.c_str());
        ImGui::Spacing();
        for (const InspectSection& s : ins.sections) {
            if (!ImGui::CollapsingHeader(s.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                continue;
            if (!ImGui::BeginTable(("##" + s.title).c_str(), 2, ImGuiTableFlags_SizingStretchProp))
                continue;
            ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthStretch, 0.38f);
            ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.62f);
            for (const InspectLine& l : s.lines) {
                ImGui::TableNextRow();
                if (l.label.empty()) {
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", l.value.c_str());
                    continue;
                }
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
                ImGui::TextWrapped("%s", l.label.c_str());
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", l.value.c_str());
            }
            ImGui::EndTable();
        }
        if (!ins.live && !ins.dead)
            note("Foto del momento del click: las bacterias y las partículas se reordenan en memoria y no se pueden seguir.");
        if (ins.hasGenome && !ins.dead && ImGui::CollapsingHeader(tr("Genoma"), ImGuiTreeNodeFlags_DefaultOpen))
            drawGenome(*app.world, ins.entity);
    }
};

} // namespace

std::unique_ptr<Panel> makeInspectorPanel() { return std::make_unique<InspectorPanel>(); }

} // namespace ui
