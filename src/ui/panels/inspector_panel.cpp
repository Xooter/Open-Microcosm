// Ficha: qué es lo elegido con la herramienta Seleccionar, qué hace y por qué.
#include "app/app.hpp"
#include "bio/genome.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "ui/glossary_ui.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <cmath>
#include <string>

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
        ImGui::TextColored(ins.dead ? ImVec4(0.9f, 0.35f, 0.3f, 1.0f) : ImVec4(1, 1, 1, 1), "%s", tr(ins.title.c_str()));
        ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
        ImGui::TextWrapped("%s", tr(ins.subtitle.c_str()));
        ImGui::PopStyleColor();
        if (ins.dead && ins.successor != entt::null)
            note("El control pasa a su sucesor.");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", tr(ins.blurb.c_str()));
        // Los términos del glosario de toda la ficha (el texto y sus secciones).
        std::string all = ins.subtitle + '\n' + ins.blurb;
        for (const InspectSection& s : ins.sections)
            for (const InspectLine& l : s.lines)
                all += '\n' + l.label + '\n' + l.value;
        glossaryChips(app, {all.c_str()});
        ImGui::Spacing();
        for (const InspectSection& s : ins.sections) {
            // Los textos fijos de la ficha se traducen; los valores con números quedan como se armaron.
            if (!ImGui::CollapsingHeader((std::string(tr(s.title.c_str())) + "###" + s.title).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                continue;
            // Las líneas sin rótulo van a todo el ancho: una tabla de ImGui no une columnas, así
            // que se corta la tabla ahí (antes quedaban apretadas en la columna del rótulo).
            bool table = false;
            int part = 0;
            for (const InspectLine& l : s.lines) {
                if (l.label.empty()) {
                    if (table)
                        ImGui::EndTable();
                    table = false;
                    ImGui::TextWrapped("%s", tr(l.value.c_str()));
                    continue;
                }
                if (!table) {
                    table = ImGui::BeginTable(("##" + s.title + std::to_string(part++)).c_str(), 2,
                                              ImGuiTableFlags_SizingStretchProp);
                    if (!table)
                        continue;
                    ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthStretch, 0.38f);
                    ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch, 0.62f);
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
                ImGui::TextWrapped("%s", tr(l.label.c_str()));
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", tr(l.value.c_str()));
            }
            if (table)
                ImGui::EndTable();
        }
        if (!ins.live && !ins.dead)
            note("Foto del momento del click: las bacterias y las partículas se reordenan en memoria y no se pueden seguir.");
        if (ins.hasGenome && !ins.dead && ImGui::CollapsingHeader(tr("Genoma"), ImGuiTreeNodeFlags_DefaultOpen))
            drawGenome(*app.world, ins.entity);
        if (st.picked.live() && !ins.dead)
            drawNotes(app);
    }

  private:
    char note_[512] = "";

    // Notas en la ficha: las de este ser y un campo para anotar (no cambian la simulación).
    void drawNotes(App& app) {
        const Picked& p = app.selection.picked;
        const std::uint32_t ref =
            p.kind == Picked::Kind::Cell || p.kind == Picked::Kind::Rock ? static_cast<std::uint32_t>(p.entity) : p.id;
        if (!ImGui::CollapsingHeader(tr("Notas"), 0))
            return;
        int k = 0;
        for (const Note& n : app.session.notes()) {
            ++k;
            if (n.anchor != NoteAnchor::Being || n.pickedKind != static_cast<int>(p.kind) || n.ref != ref)
                continue;
            ImGui::TextDisabled("%d. t = %.2f h", k, n.hours);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(n.text.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##nota", tr("Una observación sobre este ser"), note_, sizeof note_);
        ImGui::BeginDisabled(note_[0] == '\0');
        if (ImGui::SmallButton(tr("Anotar"))) {
            app.addNote(NoteAnchor::Being, note_);
            note_[0] = '\0';
        }
        ImGui::EndDisabled();
        tooltip("La nota sigue al ser mientras viva; va en la escena y en la exportación. No es una intervención.");
    }
};

} // namespace

std::unique_ptr<Panel> makeInspectorPanel() { return std::make_unique<InspectorPanel>(); }

} // namespace ui
