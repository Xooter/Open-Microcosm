#pragma once

#include "game/parameters.hpp"

struct App;

namespace ui {

// Controles de un grupo del registro de parámetros (game/parameters): rótulo, rango, unidad y
// ayuda salen de la tabla. Los cambios pasan por App::setParameter (quedan registrados).
void parameterControls(App& app, ParamGroup group);

} // namespace ui
