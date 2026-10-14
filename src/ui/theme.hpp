#pragma once

namespace ui {

// Estilo de instrumento: paneles oscuros y neutros (el campo claro del microscopio resalta) con
// un único acento verde. Todos los colores de la UI salen de acá.
void applyTheme();

struct Palette {
    float accent[4];
    float warn[4];
    float good[4];
    float dim[4];
};
const Palette& palette();

} // namespace ui
