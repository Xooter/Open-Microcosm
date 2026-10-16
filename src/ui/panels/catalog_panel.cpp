// Catálogo: todos los seres y el sedimento. Un click arma el spawn (herramienta Spawnear).
#include "app/app.hpp"
#include "ui/being_icons.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>

namespace ui {

namespace {

// Pestaña "Todos": índice kCategoryCount en la tabla (se dibuja primera).
constexpr int kAllTab = kCategoryCount;

const std::vector<int>& categoryItems(int category) {
    static const std::vector<std::vector<int>> table = [] {
        std::vector<std::vector<int>> t(kCategoryCount + 1);
        for (int i = 0; i < beingCount(); ++i) {
            t[static_cast<std::size_t>(categoryOf(beingAt(i)))].push_back(i);
            t[kAllTab].push_back(i);
        }
        return t;
    }();
    return table[static_cast<std::size_t>(category)];
}

// La rueda del mouse sobre la barra desplaza las pestañas (ImGui solo trae flechas).
void scrollTabBarWithWheel() {
    ImGuiTabBar* bar = ImGui::GetCurrentTabBar();
    const float wheel = ImGui::GetIO().MouseWheel + ImGui::GetIO().MouseWheelH;
    if (bar == nullptr || wheel == 0.0f || !ImGui::IsWindowHovered() ||
        !ImGui::IsMouseHoveringRect(bar->BarRect.Min, bar->BarRect.Max))
        return;
    bar->ScrollingTarget -= wheel * 3.0f * ImGui::GetFontSize();
}

class CatalogPanel : public Panel {
  public:
    const char* key() const override { return "catalogo"; }
    const char* title() const override { return "Catálogo"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        CatalogState& st = app.catalog;
        checkbox("Transformar al ser controlado", &st.reshape,
                 "En vez de spawnear, el ícono transforma al ser controlado en esa especie (solo especies).");
        if (st.armed >= 0)
            ImGui::TextColored({0.45f, 0.80f, 0.50f, 1.0f}, "%s: %s", tr("Armado"), tr(beingName(beingAt(st.armed))));
        else
            note("Elegí un ser y hacé click en la placa para ponerlo. Esc cancela.");
        if (!ImGui::BeginTabBar("##cats", ImGuiTabBarFlags_FittingPolicyScroll))
            return;
        scrollTabBarWithWheel();
        // "Todos" va primero: es la pestaña que se abre por defecto.
        for (int t = 0; t <= kCategoryCount; ++t) {
            const int c = t == 0 ? kAllTab : t - 1;
            const char* label = c == kAllTab ? tr("Todos") : tr(categoryName(static_cast<BeingCategory>(c)));
            if (!ImGui::BeginTabItem(label))
                continue;
            st.category = c;
            // Los íconos van en una región propia: se desplazan sin llevarse las pestañas.
            if (ImGui::BeginChild("##icons"))
                drawIcons(app, c);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

  private:
    void drawIcons(App& app, int category) {
        CatalogState& st = app.catalog;
        constexpr float kSize = 48.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        float x = 0.0f;
        for (int i : categoryItems(category)) {
            const Being b = beingAt(i);
            const bool usable = !st.reshape || b.kind == BeingKind::Species;
            if (x + kSize > width && x > 0.0f)
                x = 0.0f;
            else if (x > 0.0f)
                ImGui::SameLine();
            const IconUv uv = beingIcon(i);
            const bool armed = st.armed == i;
            ImGui::PushID(i);
            ImGui::BeginDisabled(!usable);
            if (armed)
                ImGui::PushStyleColor(ImGuiCol_Button, {0.30f, 0.62f, 0.42f, 1.0f});
            const ImVec4 bg = armed ? ImVec4(0.30f, 0.62f, 0.42f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            const bool clicked = ImGui::ImageButton("##icon", static_cast<ImTextureID>(uv.texture), {kSize, kSize},
                                                    {uv.u0, uv.v0}, {uv.u1, uv.v1}, bg, {1, 1, 1, 1});
            if (armed)
                ImGui::PopStyleColor();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tr(beingName(b)));
            ImGui::PopID();
            x += kSize + ImGui::GetStyle().ItemSpacing.x + 2.0f * ImGui::GetStyle().FramePadding.x;
            if (!clicked)
                continue;
            if (st.reshape && b.kind == BeingKind::Species) {
                const Vector2 mid = GetScreenToWorld2D({app.viewport.x + 0.5f * app.viewport.width,
                                                        app.viewport.y + 0.5f * app.viewport.height}, app.camera);
                Intervention iv; // queda en el registro de la sesión
                iv.kind = InterventionKind::PlayerSpecies;
                iv.index = static_cast<int>(b.species);
                iv.pos = {mid.x, mid.y};
                app.intervene(iv);
            } else if (st.armed == i) {
                app.setTool(Tool::Select);
            } else {
                st.armed = i;
                app.setTool(Tool::Spawn);
            }
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeCatalogPanel() { return std::make_unique<CatalogPanel>(); }

} // namespace ui
