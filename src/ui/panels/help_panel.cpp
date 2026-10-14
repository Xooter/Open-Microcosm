// Ayuda: controles y atajos.
#include "app/app.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

namespace ui {

namespace {

class HelpPanel : public Panel {
  public:
    HelpPanel() { open = false; }
    const char* key() const override { return "ayuda"; }
    const char* title() const override { return "Ayuda"; }
    Dock dock() const override { return Dock::Float; }

    void draw(App&) override {
        note("Click derecho y arrastrar: mover la cámara. Rueda: zoom.");
        note("Barra izquierda (o 1-0): herramienta activa. Con Seleccionar, un click elige el ser y abre su ficha.");
        note("Catálogo: elegí un ser y click en la placa para ponerlo. Esc cancela.");
        note("Controlar (2): click en una célula; W/S/A/D o click sostenido para moverla; R reproducir; V conjugar.");
        note("Espacio: pausa. . : un paso. - / = : velocidad. F1-F10: capas de campo. J: tinción. F: seguir con la cámara.");
        note("Medir (M): arrastrar mide µm; en modo área, un lazo da área y perímetro. Región (G): un lazo cuenta "
             "células y mide la química adentro. L: mapa de limitación. Ctrl+E: exportar datos (CSV y JSON).");
        note("Ctrl+N: crear la placa del editor. Los paneles se arrastran y acoplan; Ver > Restablecer disposición.");
        note("Ctrl+click o doble click en un deslizador: tipear el valor.");
    }
};

} // namespace

std::unique_ptr<Panel> makeHelpPanel() { return std::make_unique<HelpPanel>(); }

} // namespace ui
