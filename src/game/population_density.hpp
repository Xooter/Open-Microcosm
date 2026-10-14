#pragma once

#include <cstddef>
#include <vector>

// Densidad de una población dentro de la vista: cuántos individuos (o cuánta biomasa) caen en
// cada celda de una grilla, separada por tipo (cepa, estado). Se arma con las posiciones reales
// de la simulación; no es decorado. Sin raylib: la usa el render y se prueba en los tests.
//
// Las celdas son cuadradas, de lado potencia de 2 [µm] y ancladas al origen del mundo: al mover
// la cámara un individuo sigue cayendo en la misma celda (la imagen no tiembla).
class DensityGrid {
  public:
    // Prepara la grilla para cubrir [x0, x1) × [y0, y1) con celdas de lado >= minBin y a lo sumo
    // maxBins celdas (si no alcanza, las agranda). Reusa la memoria entre frames.
    void reset(float x0, float y0, float x1, float y1, float minBin, int maxBins, int kinds);
    // Suma un individuo; lo que cae fuera de la grilla se ignora.
    void add(float x, float y, int kind, float weight) {
        const int cx = static_cast<int>((x - originX_) * invBin_), cy = static_cast<int>((y - originY_) * invBin_);
        if (x < originX_ || y < originY_ || cx >= nx_ || cy >= ny_)
            return;
        v_[(static_cast<std::size_t>(cy) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)) * kinds_ +
           static_cast<std::size_t>(kind)] += weight;
    }
    // Suavizado binomial separable [1 2 1]/4, `passes` veces. Con bordes espejados conserva el
    // total: solo redistribuye entre celdas vecinas (los cúmulos siguen donde están).
    void smooth(int passes);

    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int kinds() const { return static_cast<int>(kinds_); }
    float binSize() const { return bin_; }
    float originX() const { return originX_; }
    float originY() const { return originY_; }
    float at(int cx, int cy, int kind) const {
        return v_[(static_cast<std::size_t>(cy) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)) * kinds_ +
                  static_cast<std::size_t>(kind)];
    }
    double sum() const;

  private:
    int nx_ = 0, ny_ = 0;
    std::size_t kinds_ = 1;
    float bin_ = 1.0f, invBin_ = 1.0f, originX_ = 0.0f, originY_ = 0.0f;
    std::vector<float> v_, tmp_;
};
