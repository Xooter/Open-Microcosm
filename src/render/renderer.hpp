#pragma once

#include "sim/rules.hpp"

#include <raylib.h>

class World;

class Renderer {
  public:
    Renderer();
    void drawWorld(const World& world, const Camera2D& camera) const;

  private:
    Color palette_[kMaxSpecies];
};
