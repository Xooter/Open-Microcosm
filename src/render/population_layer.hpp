#pragma once

#include <raylib.h>

#include <vector>

class DensityGrid;

// Representación agregada de una población (LOD 0): la grilla de densidad se dibuja como la
// turbidez que produce en campo claro. Cada celda oscurece según la fracción de su área que
// cubren los individuos (Beer–Lambert: 1 − e^−cobertura), con el color medio de sus tipos. Con
// pocos individuos coincide con el oscurecimiento medio de dibujarlos uno por uno, así el fundido
// con los individuos no cambia el tono de la imagen.
class PopulationLayer {
  public:
    PopulationLayer() = default;
    PopulationLayer(const PopulationLayer&) = delete;
    PopulationLayer& operator=(const PopulationLayer&) = delete;
    ~PopulationLayer();

    // `palette`: RGBA (0–1) por tipo, como el del instancing. `area`: área proyectada de un
    // individuo [µm²]. `gain`: contraste (1 = físico). `opacity`: peso del fundido (0–1).
    void update(const DensityGrid& grid, const float* palette, float area, float gain, float opacity);
    // Variante esquemática (vista de modelo) para lo que no absorbe luz visible, como los fagos:
    // opacidad logarítmica de la densidad por área [1/µm²] entre `lo` y `hi`, en un solo color.
    void updateLog(const DensityGrid& grid, Color color, float lo, float hi, float maxAlpha);
    // Dentro de BeginMode2D.
    void draw() const;

  private:
    void begin(const DensityGrid& grid);
    void ensureTexture(int width, int height);

    Texture2D texture_{};
    std::vector<Color> pixels_;
    float x_ = 0.0f, y_ = 0.0f, bin_ = 1.0f;
    int nx_ = 0, ny_ = 0;
    bool visible_ = false;
};
