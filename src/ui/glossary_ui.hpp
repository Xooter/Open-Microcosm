#pragma once

#include <initializer_list>

struct App;

namespace ui {

// Debajo de un texto (la ficha, un paso de una lección): los términos del glosario que aparecen en
// él, como botones chicos. Al pasar el mouse se ve la definición; un click la abre en el panel
// Glosario. Busca en el texto en español y en su traducción.
void glossaryChips(App& app, std::initializer_list<const char*> texts);
// Tooltip con el término y su definición (en el idioma de la interfaz).
void glossaryTooltip(int term);

} // namespace ui
