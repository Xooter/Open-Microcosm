#pragma once

#include <raylib.h>

#include <vector>

class World;

enum class Overlay { None, Velocity, Oxygen, Nutrients, Light };

const char* overlayName(Overlay overlay);

// Overlay de debug: pinta un campo de la grilla como textura semitransparente sobre el mundo.
class FieldOverlay {
  public:
    FieldOverlay() = default;
    FieldOverlay(const FieldOverlay&) = delete;
    FieldOverlay& operator=(const FieldOverlay&) = delete;
    ~FieldOverlay();

    void draw(const World& world, Overlay overlay);

  private:
    void ensureTexture(int width, int height);

    Texture2D texture_{};
    std::vector<Color> pixels_;
};
