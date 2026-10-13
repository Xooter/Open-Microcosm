#pragma once

#include <raylib.h>

#include <vector>

class World;

enum class Overlay { None, Velocity, Oxygen, Nutrients, Light, Phosphate, Nitrogen, PH, Toxin, Autoinducer, Phage, Silicate, Iron, Vitamin };

const char* overlayName(Overlay overlay);

// Overlay de debug: pinta un campo de la grilla como textura semitransparente sobre el mundo.
class FieldOverlay {
  public:
    FieldOverlay() = default;
    FieldOverlay(const FieldOverlay&) = delete;
    FieldOverlay& operator=(const FieldOverlay&) = delete;
    ~FieldOverlay();

    // Recalcula la textura (una vez por frame) y la dibuja en una copia del mundo.
    void update(const World& world, Overlay overlay);
    void draw(const World& world, float offsetX, float offsetY) const;

  private:
    void ensureTexture(int width, int height);

    Texture2D texture_{};
    bool visible_ = false;
    std::vector<Color> pixels_;
};
