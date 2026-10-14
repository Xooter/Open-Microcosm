#pragma once

#include <raylib.h>

#include <vector>

class Bacteria;

// Render instanciado (fase 12): todas las bacterias en una sola llamada de dibujo por copia del
// mundo toroidal. Cada instancia son 16 bytes (posición, rumbo y tipo) en un buffer que se
// reusa entre frames; el shader arma y rota el bastoncito.
class InstancedParticles {
  public:
    InstancedParticles() = default;
    InstancedParticles(const InstancedParticles&) = delete;
    InstancedParticles& operator=(const InstancedParticles&) = delete;
    ~InstancedParticles();

    bool ready();
    // Carga las instancias (una vez por frame).
    void upload(const Bacteria& bacteria);
    // Dibuja todas las instancias con el desplazamiento dado (dentro de BeginMode2D).
    void draw(float offsetX, float offsetY, float length, float thickness);
    // Color (RGBA 0-1) de cada tipo de bacteria: 8 tipos.
    void setPalette(const float rgba[32]) { palette_.assign(rgba, rgba + 32); }

  private:
    void init();

    bool tried_ = false, ok_ = false;
    Shader shader_{};
    unsigned int vao_ = 0, quadVbo_ = 0, instanceVbo_ = 0;
    int capacity_ = 0, count_ = 0;
    int mvpLoc_ = -1, halfSizeLoc_ = -1, paletteLoc_ = -1;
    std::vector<float> data_;
    std::vector<float> palette_;
};
