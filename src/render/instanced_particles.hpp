#pragma once

#include "render/lod.hpp"

#include <raylib.h>

#include <vector>

// Render instanciado (fase 12): los individuos visibles de una población (bacterias,
// Bdellovibrio) en una sola llamada de dibujo. Cada instancia son 16 bytes (posición, rumbo y
// tipo) en un buffer que se reusa entre frames; el shader arma y rota el bastoncito. Solo se
// cargan las instancias que pasaron el recorte por la vista (ya con el desplazamiento de su copia
// del mundo toroidal).
class InstancedParticles {
  public:
    InstancedParticles() = default;
    InstancedParticles(const InstancedParticles&) = delete;
    InstancedParticles& operator=(const InstancedParticles&) = delete;
    ~InstancedParticles();

    bool ready();
    // Arma las instancias del frame: clear(), push() por cada individuo visible y upload().
    void clear() { count_ = 0; data_.clear(); }
    void push(float x, float y, float heading, float kind) {
        data_.insert(data_.end(), {x, y, heading, kind});
        ++count_;
    }
    void upload();
    int count() const { return count_; }
    const std::vector<float>& data() const { return data_; } // (x, y, rumbo, tipo) por instancia
    // Dibuja las instancias (dentro de BeginMode2D). `fade` multiplica la opacidad (fundido con
    // la densidad); `level` elige la forma; `pixel` es el tamaño de un píxel [µm] para el borde.
    void draw(float length, float thickness, float fade, DetailLevel level, float pixel);
    // Color (RGBA 0-1) de cada tipo de bacteria: 8 tipos.
    void setPalette(const float rgba[32]) { palette_.assign(rgba, rgba + 32); }

  private:
    void init();

    bool tried_ = false, ok_ = false;
    Shader shader_{};
    unsigned int vao_ = 0, quadVbo_ = 0, instanceVbo_ = 0;
    int capacity_ = 0, count_ = 0;
    int mvpLoc_ = -1, halfSizeLoc_ = -1, paletteLoc_ = -1, fadeLoc_ = -1, levelLoc_ = -1, pixelLoc_ = -1;
    std::vector<float> data_;
    std::vector<float> palette_;
};
