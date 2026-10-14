// Herramientas: parámetros de cada una (radios, gotero) y capas de visualización.
#include "app/app.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

namespace ui {

namespace {

class ToolsPanel : public Panel {
  public:
    const char* key() const override { return "herramientas"; }
    const char* title() const override { return "Herramientas"; }
    Dock dock() const override { return Dock::RightLow; }

    void draw(App& app) override {
        ToolState& t = app.tools;
        const ToolInfo& info = toolInfo(t.tool);
        ImGui::TextColored({0.36f, 0.72f, 0.52f, 1.0f}, "%s", tr(info.name));
        note(info.help);
        ImGui::Separator();
        if (section("Parámetros", true)) {
            slider("Radio de revolver [µm]", &t.stirRadius, 5.0f, 120.0f, "%.0f");
            slider("Radio de tinta [µm]", &t.inkRadius, 2.0f, 60.0f, "%.0f");
            slider("Apretar [µm/s]", &t.squeezeSpeed, 5.0f, 120.0f, "%.0f");
        }
        if (section("Gotero", true)) {
            int d = t.dropper;
            std::vector<const char*> names;
            for (int i = 0; i < dropperCount(); ++i)
                names.push_back(dropperName(i));
            if (combo("Soluto", &d, names.data(), dropperCount()))
                t.dropper = d;
            slider("Radio [µm]", &t.dropRadius, 10.0f, 200.0f, "%.0f");
            sliderLog("Cantidad", &t.dropAmount, 0.1f, 20.0f, "x%.1f", "Relativa a la concentración típica del soluto.");
        }
        if (section("Visualización", true)) {
            const int overlayCount = static_cast<int>(Overlay::Vitamin) + 1;
            std::vector<const char*> overlays;
            for (int i = 0; i < overlayCount; ++i)
                overlays.push_back(overlayName(static_cast<Overlay>(i)));
            int o = static_cast<int>(t.overlay);
            if (combo("Capa de campo", &o, overlays.data(), overlayCount, "Pinta un campo de la placa sobre el mundo (F1-F10)."))
                t.overlay = static_cast<Overlay>(o);
            static const char* const kStains[] = {"Sin tinción", "Rojo Congo", "Azul de metileno"};
            combo("Tinción", &t.stain, kStains, 3, "Cambia cómo se ven las células (J).");
            checkbox("La cámara sigue al ser controlado", &t.follow);
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeToolsPanel() { return std::make_unique<ToolsPanel>(); }

} // namespace ui
