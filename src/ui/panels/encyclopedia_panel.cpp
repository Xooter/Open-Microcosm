// Enciclopedia: se llena con lo que la cámara ve y con los comportamientos observados.
#include "app/app.hpp"
#include "game/encyclopedia.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

namespace ui {

namespace {

class EncyclopediaPanel : public Panel {
  public:
    EncyclopediaPanel() { open = false; }
    const char* key() const override { return "encyclopedia"; }
    const char* title() const override { return "Enciclopedia"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        const Progress& pr = app.progress;
        int seen = 0;
        for (int k = 0; k < speciesCount(); ++k)
            seen += (pr.seenSpecies >> k) & 1u;
        ImGui::Text("%s: %d/%d", tr("Especies vistas"), seen, speciesCount());
        ImGui::Separator();
        if (section("Especies", true)) {
            for (int k = 0; k < speciesCount(); ++k) {
                const auto id = static_cast<SpeciesId>(k);
                if ((pr.seenSpecies >> k) & 1u) {
                    if (ImGui::TreeNode(speciesInfo(id).name)) {
                        ImGui::TextWrapped("%s", speciesBlurb(id));
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::TextDisabled("???  (%s)", tr("todavía no la viste"));
                }
            }
        }
        if (section("Otros organismos")) {
            for (int k = 0; k < kOtherCount; ++k) {
                const auto o = static_cast<OtherOrganism>(k);
                if ((pr.seenOthers >> k) & 1u) {
                    if (ImGui::TreeNode(otherName(o))) {
                        ImGui::TextWrapped("%s", otherBlurb(o));
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::TextDisabled("???");
                }
            }
        }
        if (section("Comportamientos observados")) {
            ImGui::Text("%zu", pr.behaviors.size());
            for (const std::string& b : pr.behaviors)
                ImGui::BulletText("%s", b.c_str());
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeEncyclopediaPanel() { return std::make_unique<EncyclopediaPanel>(); }

} // namespace ui
