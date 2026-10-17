#pragma once

#include <imgui.h>

namespace ui {

// Estilo de instrumento: paneles oscuros y neutros (el campo claro del microscopio resalta) con
// un único acento verde. Todos los colores de la UI salen de acá.
void applyTheme();

// Los colores con significado (bien, aviso, muerto) son de Okabe–Ito (core/palette): se distinguen
// también con daltonismo. El acento es solo estructural.
struct Palette {
    float accent[4];
    float warn[4];
    float good[4];
    float dim[4];
    float bad[4];
};
const Palette& palette();
inline ImVec4 col(const float (&a)[4]) { return ImVec4(a[0], a[1], a[2], a[3]); }

// Escala de la interfaz (texto, controles y espaciados): la de las opciones o la del monitor.
float uiScale();
void setUiScale(float scale); // fija la escala que usan applyTheme y los tamaños en píxeles

} // namespace ui
