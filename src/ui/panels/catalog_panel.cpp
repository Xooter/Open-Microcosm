// Catálogo: todos los seres y el sedimento. Un click arma el spawn (herramienta Spawnear).
#include "app/app.hpp"
#include "game/glossary.hpp"
#include "ui/being_icons.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

namespace {

// Pestaña "Todos": índice kCategoryCount en la tabla (se dibuja primera).
constexpr int kAllTab = kCategoryCount;

const std::vector<int>& categoryItems(int category) {
    // Se rehace si un paquete importado sumó seres.
    static std::vector<std::vector<int>> table;
    static int built = -1;
    if (built != beingCount()) {
        built = beingCount();
        table.assign(kCategoryCount + 1, {});
        for (int i = 0; i < built; ++i) {
            table[static_cast<std::size_t>(categoryOf(beingAt(i)))].push_back(i);
            table[kAllTab].push_back(i);
        }
    }
    return table[static_cast<std::size_t>(category)];
}

// Para buscar: minúsculas y sin tildes (Latin-1 en UTF-8), así "celula" encuentra "Célula".
std::string searchKey(std::string_view text) {
    std::string folded = foldCase(text);
    std::string out;
    out.reserve(folded.size());
    for (std::size_t i = 0; i < folded.size(); ++i) {
        const auto c = static_cast<unsigned char>(folded[i]);
        if (c != 0xC3 || i + 1 >= folded.size()) {
            out += folded[i];
            continue;
        }
        const auto d = static_cast<unsigned char>(folded[i + 1]);
        char plain = 0;
        if (d >= 0xA0 && d <= 0xA5) plain = 'a';
        else if (d == 0xA7) plain = 'c';
        else if (d >= 0xA8 && d <= 0xAB) plain = 'e';
        else if (d >= 0xAC && d <= 0xAF) plain = 'i';
        else if (d == 0xB1) plain = 'n';
        else if (d >= 0xB2 && d <= 0xB6) plain = 'o';
        else if (d >= 0xB9 && d <= 0xBC) plain = 'u';
        if (plain != 0) {
            out += plain;
            ++i;
        } else
            out += folded[i];
    }
    return out;
}

// ¿El ser coincide con la búsqueda? Por su nombre en español, en el idioma de la interfaz o por su
// id de los datos ("paramecium").
bool matchesSearch(Being b, const std::string& wanted) {
    const char* name = beingName(b);
    if (searchKey(name).find(wanted) != std::string::npos ||
        searchKey(tr(name)).find(wanted) != std::string::npos)
        return true;
    return b.kind == BeingKind::Species && searchKey(speciesInfo(b.species).key).find(wanted) != std::string::npos;
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
    // Inocular el ser armado: muchos individuos de una vez, repartidos al azar (una intervención
    // reproducible, como las demás).
    static void drawInoculate(App& app, CatalogState& st) {
        const Being b = beingAt(st.armed);
        const bool cells = b.kind == BeingKind::Species || b.kind == BeingKind::Bacterium ||
                           b.kind == BeingKind::Bacillus || b.kind == BeingKind::Violaceum ||
                           b.kind == BeingKind::Spirillum;
        if (!cells || !app.world || !ImGui::TreeNode("##inocular", "%s", tr("Inocular...")))
            return;
        checkbox("Por densidad", &st.byDensity, "Individuos por cm² del área donde se inocula (como se siembra un cultivo).");
        checkbox("Alrededor del centro de la vista", &st.spot, "Si no, en toda la placa.");
        if (st.spot)
            slider("Radio [µm]", &st.spotRadius, 20.0f, 2000.0f, "%.0f");
        const World& w = *app.world;
        float areaCm2 = w.width() * w.height() * 1e-8f;
        if (st.spot)
            areaCm2 = 3.14159265f * st.spotRadius * st.spotRadius * 1e-8f;
        else if (w.dish().active())
            areaCm2 = 3.14159265f * w.dish().radius * w.dish().radius * 1e-8f;
        int count = st.inoculateCount;
        checkbox("De una suspensión", &st.bySuspension,
                 "Un volumen de una suspensión de densidad conocida: la cantidad que entra es un sorteo de Poisson "
                 "(una suspensión de 10 por mL rara vez deja un individuo en 1 µL).");
        if (st.bySuspension) {
            sliderLog("Suspensión [por mL]", &st.suspensionPerMl, 1.0f, 1e10f, "%.3g");
            sliderLog("Inóculo [µL]", &st.inoculumUl, 0.01f, 100.0f, "%.2f");
            const double expected = static_cast<double>(st.suspensionPerMl) * st.inoculumUl * 1e-3;
            readout("Esperados", "%.2f (± %.2f)", expected, std::sqrt(expected));
            // Se sortea al apretar; la cantidad sorteada queda en la intervención (la escena la repite).
            Rng rng(w.config().seed, rng_stream::kTools + 77, st.draws, w.stepIndex());
            count = static_cast<int>(rng.poisson(expected));
        } else if (st.byDensity) {
            sliderLog("Densidad [1/cm²]", &st.density, 10.0f, 1e7f, "%.0f");
            count = static_cast<int>(std::round(st.density * areaCm2));
            readout("Cantidad", "%d", count);
        } else {
            sliderInt("Cantidad", &st.inoculateCount, 1, 2000);
            readout("Densidad", "%.0f /cm²", static_cast<double>(static_cast<float>(count) / areaCm2));
        }
        // Tope: una inoculación no puede colgar la placa (cada célula es un cuerpo blando).
        count = std::min(count, b.kind == BeingKind::Species ? 2000 : 200000);
        if (ImGui::Button(tr("Inocular")) && count > 0) {
            Intervention iv;
            iv.kind = InterventionKind::Inoculate;
            iv.index = static_cast<int>(b.kind);
            iv.v[0] = static_cast<float>(static_cast<int>(b.species));
            iv.v[1] = static_cast<float>(count);
            iv.v[2] = st.spot ? st.spotRadius : 0.0f;
            const Vector2 c = GetScreenToWorld2D({app.viewport.x + 0.5f * app.viewport.width, app.viewport.y + 0.5f * app.viewport.height},
                                                 app.camera);
            iv.pos = {c.x, c.y};
            app.intervene(iv);
            ++st.draws;
        }
        ImGui::TreePop();
    }

  public:
    const char* key() const override { return "catalog"; }
    const char* title() const override { return "Catálogo"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        CatalogState& st = app.catalog;
        checkbox("Transformar al ser controlado", &st.reshape,
                 "En vez de spawnear, el ícono transforma al ser controlado en esa especie (solo especies).");
        if (st.armed >= 0) {
            ImGui::TextColored({0.45f, 0.80f, 0.50f, 1.0f}, "%s: %s", tr("Armado"), tr(beingName(beingAt(st.armed))));
            drawInoculate(app, st);
        } else
            note("Elegí un ser y hacé click en la placa para ponerlo. Esc cancela.");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##buscar", tr("Buscar"), filter_, sizeof filter_);
        const std::string wanted = searchKey(filter_);
        // Con una búsqueda, los resultados de todas las categorías en vez de las pestañas.
        if (!wanted.empty()) {
            found_.clear();
            for (int i : categoryItems(kAllTab))
                if (matchesSearch(beingAt(i), wanted))
                    found_.push_back(i);
            if (found_.empty())
                note("Ningún ser coincide con la búsqueda.");
            if (ImGui::BeginChild("##icons"))
                drawIcons(app, found_);
            ImGui::EndChild();
            return;
        }
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
                drawIcons(app, categoryItems(c));
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

  private:
    char filter_[64] = "";
    std::vector<int> found_; // resultados de la búsqueda (se reusa entre cuadros)

    void drawIcons(App& app, const std::vector<int>& items) {
        CatalogState& st = app.catalog;
        constexpr float kSize = 48.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        float x = 0.0f;
        for (int i : items) {
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
