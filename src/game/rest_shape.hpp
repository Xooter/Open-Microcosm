#pragma once

#include "core/vec2.hpp"
#include "sim/soft_body.hpp"

#include <vector>

// Forma de reposo que cambia mientras la célula vive (contracción de Stentor y los peritricos, metabolia
// de Euglena, vaivén de la probóscide): lo común a todos esos mecanismos.
namespace rest_shape {

// Pone `shape` (en el marco local de la célula, punto a punto con el anillo, en cualquier escala) como
// forma de reposo. El área de reposo pasa a ser la actual por `areaScale / previousAreaScale` (la
// silueta que crece al extenderse; 1 y 1 la conservan). La forma queda centrada en el promedio de sus
// puntos, que es el centro de soft_body::bestFit: descentrada, la memoria de forma empujaría al cuerpo
// entero. Rehace los largos de las aristas y acomoda los organelos: cada uno conserva la fracción de
// volumen a lo largo del eje y su lugar relativo a lo ancho (el citoplasma fluye con la forma).
void apply(RingBody& body, const std::vector<Vec2>& shape, float areaScale = 1.0f, float previousAreaScale = 1.0f);

} // namespace rest_shape
