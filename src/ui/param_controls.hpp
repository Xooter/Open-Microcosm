#pragma once

#include "game/parameters.hpp"

struct App;

namespace ui {

// Controles de un grupo del registro de parámetros (game/parameters): rótulo, rango, unidad y
// ayuda salen de la tabla. Los cambios pasan por App::setParameter (quedan registrados).
void parameterControls(App& app, ParamGroup group);
// Un solo parámetro. Si el protocolo en curso lo fija (App::protocolLocks), se ve trabado con su
// motivo; destrabarlo es posible y queda anotado: la corrida sale "fuera de protocolo".
void parameterControl(App& app, const ParamInfo& p);

} // namespace ui
