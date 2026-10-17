// Notas (fase P1, paso 14): observaciones del usuario ancladas a un ser, a un lugar o a un instante
// de la placa. Van en la escena y en la exportación (CSV/JSON y un Markdown), pero no son
// intervenciones: no cambian la corrida.
#include "app/app.hpp"
#include "game/notes.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <string>

namespace ui {

namespace {

class NotesPanel : public Panel {
  public:
    NotesPanel() { open = false; } // se abre desde Ver
    const char* key() const override { return "notas"; }
    const char* title() const override { return "Notas"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        if (!app.world)
            return;
        static const char* const kAnchors[] = {"Al instante", "Al lugar del centro de la vista", "Al ser elegido"};
        combo("Anclar", &anchor_, kAnchors, 3,
              "Al instante: solo el tiempo. Al lugar: el centro de lo que se ve. Al ser elegido: lo sigue mientras viva.");
        const bool beingOk = app.selection.picked.live() && app.selection.inspection.valid && !app.selection.inspection.dead;
        if (anchor_ == 2 && !beingOk)
            note("Elegí un ser con la herramienta Seleccionar (1).");
        ImGui::InputTextMultiline("##nota", text_, sizeof text_, {-1.0f, 3.5f * ImGui::GetTextLineHeightWithSpacing()});
        ImGui::BeginDisabled(text_[0] == '\0' || (anchor_ == 2 && !beingOk));
        if (ImGui::Button(tr("Agregar nota"))) {
            app.addNote(static_cast<NoteAnchor>(anchor_), text_);
            text_[0] = '\0';
        }
        ImGui::EndDisabled();
        tooltip("Las notas no cambian la simulación: van en la escena (Guardar escena) y en la exportación (Ctrl+E).");
        ImGui::Separator();
        std::vector<Note>& notes = app.session.notes();
        if (notes.empty()) {
            note("Todavía no hay notas.");
            return;
        }
        int erase = -1;
        for (std::size_t i = 0; i < notes.size(); ++i) {
            const Note& n = notes[i];
            ImGui::PushID(static_cast<int>(i));
            const bool pending = n.step > app.world->stepIndex(); // de una escena que se está repitiendo
            if (static_cast<int>(i) == app.focusNote)
                ImGui::SetScrollHereY();
            ImGui::TextColored(pending ? ImVec4(0.55f, 0.55f, 0.55f, 1.0f) : ImVec4(1, 1, 1, 1), "%zu. t = %.2f h  %s", i + 1,
                               n.hours, n.being.empty() ? "" : tr(n.being.c_str()));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(n.text.c_str());
            ImGui::PopTextWrapPos();
            Vec2 at;
            if (!pending && notePosition(*app.world, n, at)) {
                if (ImGui::SmallButton(tr("Ir"))) {
                    app.camera.target = {at.x, at.y};
                    app.camera.offset = {app.viewport.x + 0.5f * app.viewport.width,
                                         app.viewport.y + 0.5f * app.viewport.height};
                    app.tools.follow = app.tools.followSelected = false;
                }
                ImGui::SameLine();
            } else if (n.anchor == NoteAnchor::Being && !pending) {
                ImGui::TextDisabled("%s", tr("(ya no está)"));
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(tr("Borrar")))
                erase = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        app.focusNote = -1;
        if (erase >= 0)
            notes.erase(notes.begin() + erase);
    }

  private:
    int anchor_ = 0;
    char text_[1024] = "";
};

} // namespace

std::unique_ptr<Panel> makeNotesPanel() { return std::make_unique<NotesPanel>(); }

} // namespace ui
