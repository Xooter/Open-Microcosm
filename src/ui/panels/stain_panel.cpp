// Editor de tinciones: elegir, ajustar y guardar colorantes, con vista previa sobre un espécimen
// de prueba (sin teñir y teñido) dibujada por el mismo render que la placa.
#include "app/app.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <filesystem>
#include <functional>

namespace ui {

namespace {

std::string slug(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!out.empty() && out.back() != '_')
            out += '_';
    }
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    return out.empty() ? "tincion" : out;
}

const StainSpec* factoryOf(const std::string& id) {
    for (const StainSpec& s : builtinStains())
        if (s.id == id)
            return &s;
    return nullptr;
}

void drawPreview(App& app, const StainSpec& spec) {
    static int viewIndex = 0;
    static bool showUnstained = true;
    const std::size_t revision = std::hash<std::string>{}(serializeStain(spec));
    if (ImGui::BeginTabBar("##preview")) {
        if (ImGui::BeginTabItem(tr("Células"))) {
            viewIndex = StainPreview::Cells;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Detalle"))) {
            viewIndex = StainPreview::Small;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("Bacterias"))) {
            viewIndex = StainPreview::Bacteria;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    checkbox("Mostrar también sin teñir", &showUnstained);
    const auto view = static_cast<StainPreview::View>(viewIndex);
    const float w = std::min(ImGui::GetContentRegionAvail().x, static_cast<float>(StainPreview::kWidth));
    const float h = w * StainPreview::kHeight / StainPreview::kWidth;
    auto image = [&](const RenderTexture2D& t, const char* caption) {
        ImGui::TextDisabled("%s", tr(caption));
        // Las render textures de OpenGL están invertidas en Y.
        ImGui::Image(static_cast<ImTextureID>(t.texture.id), {w, h}, {0, 1}, {1, 0});
    };
    if (showUnstained)
        image(app.stainPreview.render(view, nullptr, 0), "Sin teñir");
    image(app.stainPreview.render(view, &spec, revision), spec.name.empty() ? "Teñida" : spec.name.c_str());
    if (viewIndex == StainPreview::Cells)
        note("Paramecium, Amoeba y un filamento de Spirogyra.");
    else if (viewIndex == StainPreview::Small)
        note("Arriba: Chlamydomonas, Euglena y Candida (levadura). Abajo: Navicula y Pediastrum.");
    else
        note("De izquierda a derecha: bacilos gramnegativos, Bacillus subtilis (grampositivo) y Chromobacterium violaceum.");
}

void drawRule(StainSpec& spec, std::size_t index, bool& removed) {
    StainRule& r = spec.rules[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::Separator();
    // Blanco.
    {
        const char* names[static_cast<int>(StainTarget::Count)];
        for (int i = 0; i < static_cast<int>(StainTarget::Count); ++i)
            names[i] = stainTargetName(static_cast<StainTarget>(i));
        int t = static_cast<int>(r.target);
        if (combo("Blanco", &t, names, static_cast<int>(StainTarget::Count), "Qué estructura o compuesto se tiñe."))
            r.target = static_cast<StainTarget>(t);
    }
    {
        const char* names[] = {"Fijo", "Indicador de pH", "Retención (Gram)"};
        int resp = static_cast<int>(r.response);
        if (combo("Respuesta", &resp, names, 3, "Fijo: siempre el mismo color. Indicador: vira con el pH. Retención: pierde el colorante con el decolorante."))
            r.response = static_cast<StainResponse>(resp);
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(r.response == StainResponse::Fixed ? "Color" : (r.response == StainResponse::PhIndicator ? "Color ácido / alcalino" : "Color que retiene / contratinción")));
    ImGui::SameLine();
    ImGui::ColorEdit3("##c1", &r.color.r, ImGuiColorEditFlags_NoInputs);
    if (r.response != StainResponse::Fixed) {
        ImGui::SameLine();
        ImGui::ColorEdit3("##c2", &r.color2.r, ImGuiColorEditFlags_NoInputs);
    }
    slider("Intensidad", &r.strength, 0.0f, 1.0f, "%.2f", "Opacidad del colorante sobre el blanco.");
    if (r.response == StainResponse::PhIndicator) {
        slider("pH del viraje", &r.pivot, 2.0f, 11.0f, "%.1f");
        slider("Ancho del viraje", &r.width, 0.1f, 3.0f, "%.1f", "Unidades de pH en que cambia de un color al otro.");
    } else if (r.response == StainResponse::GramRetention) {
        sliderLog("Constante de pérdida [s]", &r.tau, 0.5f, 300.0f, "%.1f",
                  "Segundos de decolorante para perder 63 % del colorante. La pared fina lo pierde en unos 3 s; la gruesa, en ~60 s.");
    }
    if (ImGui::SmallButton(tr("Quitar regla")))
        removed = true;
    ImGui::PopID();
}

class StainPanel : public Panel {
  public:
    const char* key() const override { return "tinciones"; }
    const char* title() const override { return "Tinciones"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        if (app.stains.empty())
            return;
        // --- Elegir ---
        std::vector<const char*> names;
        for (const StainSpec& s : app.stains)
            names.push_back(s.name.c_str());
        int idx = app.stainIndex;
        if (combo("Tinción activa", &idx, names.data(), static_cast<int>(names.size()), "La que se ve sobre la placa (J pasa a la siguiente)."))
            app.setStainIndex(idx);
        StainSpec& spec = app.stains[static_cast<std::size_t>(app.stainIndex)];
        const bool factory = factoryOf(spec.id) != nullptr;

        if (app.stainIndex > 0) {
            drawPreview(app, spec);
            if (!spec.note.empty())
                note(spec.note.c_str());
            const ImVec4 col = spec.vital ? ImVec4(0.45f, 0.80f, 0.50f, 1.0f) : ImVec4(0.92f, 0.60f, 0.25f, 1.0f);
            ImGui::TextColored(col, "%s", tr(spec.vital ? "Vital: no mata a la célula." : "No vital: en la realidad fija o mata a las células."));
            if (!spec.vital && app.world) {
                // Sin fijar, la tinción solo cambia los colores; fijar mata de verdad a todo lo vivo.
                if (app.world->fixed()) {
                    note("Muestra fijada: todo murió y quedó como estaba. Crear placa (Ctrl+N) para empezar otra.");
                } else {
                    if (ImGui::Button(tr("Fijar la muestra")))
                        app.world->fix();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", tr("Como en el preparado real: el calor, el yodo o el alcohol matan y fijan a todos los seres de la placa. No se puede deshacer."));
                    note("Sin fijar, la tinción solo cambia los colores y la placa sigue viva.");
                }
            }
            if (section("Parámetros", true))
                drawParams(app, spec);
            if (section("Reglas de color"))
                drawRules(spec);
            if (section("Guardar y duplicar", true))
                drawFiles(app, spec, factory);
        } else {
            note("Elegí una tinción para ver su vista previa y ajustarla.");
        }
    }

  private:
    void drawParams(App&, StainSpec& spec) {
        if (!factoryOf(spec.id)) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%s", spec.name.c_str());
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(tr("Nombre"));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##name", buf, sizeof buf))
                spec.name = buf;
        }
        int mode = static_cast<int>(spec.mode);
        const char* modes[] = {"Campo claro", "Fluorescencia"};
        if (combo("Modo", &mode, modes, 2, "Campo claro: el colorante absorbe luz. Fluorescencia: fondo oscuro y solo emite lo teñido."))
            spec.mode = static_cast<StainMode>(mode);
        slider("Concentración", &spec.dose, 0.0f, 3.0f, "x%.2f", "Multiplica la intensidad de todas las reglas.");
        slider("Decolorante / exposición [s]", &spec.time, 0.0f, 120.0f, "%.0f",
               "Gram: tiempo de alcohol. Pasarse vuelve rosa a la grampositiva; quedarse corto deja violeta a la gramnegativa.");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Fondo"));
        ImGui::SameLine();
        ImGui::ColorEdit3("##bg", &spec.background.r, ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##bgs", &spec.backgroundStrength, 0.0f, 1.0f, "%.2f");
        tooltip("Cuánto se tiñe el fondo del campo (en fluorescencia, la oscuridad).");
        checkbox("Vital (no mata a la célula)", &spec.vital);
    }

    void drawRules(StainSpec& spec) {
        std::size_t removeAt = spec.rules.size();
        for (std::size_t i = 0; i < spec.rules.size(); ++i) {
            bool removed = false;
            drawRule(spec, i, removed);
            if (removed)
                removeAt = i;
        }
        if (removeAt < spec.rules.size())
            spec.rules.erase(spec.rules.begin() + static_cast<std::ptrdiff_t>(removeAt));
        ImGui::Separator();
        if (ImGui::Button(tr("Agregar regla")))
            spec.rules.push_back(StainRule{});
        note("Cada regla tiñe un blanco; varias reglas sobre el mismo blanco se apilan. Los blancos salen de la bioquímica real: por ejemplo, solo las algas verdes guardan almidón y solo Bacillus tiene pared grampositiva.");
    }

    void drawFiles(App& app, StainSpec& spec, bool factory) {
        if (factory) {
            if (ImGui::Button(tr("Restablecer"))) {
                if (const StainSpec* f = factoryOf(spec.id))
                    spec = *f;
            }
            tooltip("Vuelve a los valores de fábrica.");
            ImGui::SameLine();
        }
        if (ImGui::Button(tr("Duplicar como mía"))) {
            StainSpec copy = spec;
            copy.name += " (mía)";
            copy.id = uniqueId(app, slug(copy.name));
            if (saveStain(copy, app.stainsDir + "/" + copy.id + ".stain")) {
                app.reloadStains();
                for (std::size_t i = 0; i < app.stains.size(); ++i)
                    if (app.stains[i].id == copy.id)
                        app.setStainIndex(static_cast<int>(i));
            }
        }
        tooltip("Guarda una copia editable en tu carpeta de tinciones.");
        if (!factory) {
            ImGui::SameLine();
            if (ImGui::Button(tr("Guardar"))) {
                saveStain(spec, app.stainsDir + "/" + spec.id + ".stain");
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Eliminar"))) {
                std::error_code ec;
                std::filesystem::remove(app.stainsDir + "/" + spec.id + ".stain", ec);
                app.reloadStains();
            }
        }
    }

    static std::string uniqueId(const App& app, const std::string& base) {
        std::string id = base;
        for (int n = 2;; ++n) {
            bool clash = false;
            for (const StainSpec& s : app.stains)
                clash |= s.id == id;
            if (!clash)
                return id;
            id = base + "_" + std::to_string(n);
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeStainPanel() { return std::make_unique<StainPanel>(); }

} // namespace ui
