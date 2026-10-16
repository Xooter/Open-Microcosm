#pragma once

#include <raylib.h>

class World;
struct StainSpec;

// Preparados (fase 20c): las paredes de un tejido vegetal con su contenido fijo (granos de almidón,
// núcleos y cromosomas en cada fase de la mitosis, estomas) y la sangre (glóbulos rojos y plaquetas).
// Se dibujan en coordenadas del mundo, desplazadas por la copia (offX, offY); `minSize`: tamaño
// mínimo visible [µm]; `px`: un píxel en µm.
void drawTissue(const World& world, float offX, float offY, Rectangle view, float minSize, float px, float time,
                const StainSpec* stain);
void drawBlood(const World& world, float offX, float offY, Rectangle view, float minSize, float px, const StainSpec* stain);
