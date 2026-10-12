#pragma once

#include "render/field_overlay.hpp"

#include <raylib.h>

class World;

// Estilo campo claro (brightfield): fondo luminoso, organismos oscuros y semitransparentes.
class Renderer {
  public:
    void drawWorld(const World& world, const Camera2D& camera, Overlay overlay);

  private:
    FieldOverlay overlay_;
};
