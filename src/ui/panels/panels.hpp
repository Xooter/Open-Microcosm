#pragma once

#include "ui/panel.hpp"

namespace ui {

// Registra todos los paneles del laboratorio. Para sumar uno: su archivo en ui/panels/ y una
// línea acá (en panels.cpp).
void registerPanels(PanelHost& host);

} // namespace ui
