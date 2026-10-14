#include "render/population_layer.hpp"

#include "core/profiling.hpp"
#include "game/population_density.hpp"

#include <algorithm>
#include <cmath>

PopulationLayer::~PopulationLayer() {
    if (texture_.id != 0)
        UnloadTexture(texture_);
}

// La textura solo crece: al cambiar el zoom cambia el tamaño de la grilla y se actualiza un
// sub-rectángulo, sin recrear la textura en cada paso de la rueda.
void PopulationLayer::ensureTexture(int width, int height) {
    if (texture_.id != 0 && texture_.width >= width && texture_.height >= height)
        return;
    const int w = std::max(width, texture_.id != 0 ? texture_.width : 0);
    const int h = std::max(height, texture_.id != 0 ? texture_.height : 0);
    if (texture_.id != 0)
        UnloadTexture(texture_);
    Image img = GenImageColor(w, h, BLANK);
    texture_ = LoadTextureFromImage(img);
    UnloadImage(img);
    // Bilineal: entre celdas la densidad varía suave, sin bloques.
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(texture_, TEXTURE_WRAP_CLAMP);
}

void PopulationLayer::begin(const DensityGrid& grid) {
    nx_ = grid.nx();
    ny_ = grid.ny();
    x_ = grid.originX();
    y_ = grid.originY();
    bin_ = grid.binSize();
    ensureTexture(nx_, ny_);
    pixels_.resize(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_));
}

void PopulationLayer::updateLog(const DensityGrid& grid, Color color, float lo, float hi, float maxAlpha) {
    PROFILE_SCOPE("PopulationLayer::updateLog");
    visible_ = true;
    begin(grid);
    const float area = bin_ * bin_, l0 = std::log10(lo), span = std::log10(hi) - l0;
    for (int cy = 0; cy < ny_; ++cy)
        for (int cx = 0; cx < nx_; ++cx) {
            const float perArea = grid.at(cx, cy, 0) / area;
            const float t = perArea > 0.0f ? std::clamp((std::log10(perArea) - l0) / span, 0.0f, 1.0f) : 0.0f;
            Color c = color;
            c.a = static_cast<unsigned char>(255.0f * maxAlpha * t + 0.5f);
            pixels_[static_cast<std::size_t>(cy) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)] = c;
        }
    UpdateTextureRec(texture_, {0, 0, static_cast<float>(nx_), static_cast<float>(ny_)}, pixels_.data());
}

void PopulationLayer::update(const DensityGrid& grid, const float* palette, float area, float gain, float opacity) {
    PROFILE_SCOPE("PopulationLayer::update");
    visible_ = opacity > 0.0f;
    if (!visible_)
        return;
    begin(grid);
    const int kinds = grid.kinds();
    const float perBin = area / (bin_ * bin_); // cobertura que aporta cada individuo
    // Las celdas vacías llevan el color del tipo 0 (con alfa 0) para que el filtro bilineal no
    // oscurezca los bordes de las manchas mezclando con negro.
    const auto u8 = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    const Color empty = {u8(palette[0]), u8(palette[1]), u8(palette[2]), 0};
    for (int cy = 0; cy < ny_; ++cy)
        for (int cx = 0; cx < nx_; ++cx) {
            float count = 0.0f, a = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f;
            for (int k = 0; k < kinds; ++k) {
                const float w = grid.at(cx, cy, k);
                const float wa = w * palette[4 * k + 3];
                count += w;
                a += wa;
                r += wa * palette[4 * k];
                g += wa * palette[4 * k + 1];
                b += wa * palette[4 * k + 2];
            }
            Color& px = pixels_[static_cast<std::size_t>(cy) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)];
            if (count <= 0.0f || a <= 0.0f) {
                px = empty;
                continue;
            }
            const float inv = 1.0f / a;
            const float alpha = (a / count) * (1.0f - std::exp(-gain * count * perBin)) * opacity;
            px = {u8(r * inv), u8(g * inv), u8(b * inv), u8(alpha)};
        }
    UpdateTextureRec(texture_, {0, 0, static_cast<float>(nx_), static_cast<float>(ny_)}, pixels_.data());
}

void PopulationLayer::draw() const {
    if (!visible_ || nx_ == 0)
        return;
    DrawTexturePro(texture_, {0, 0, static_cast<float>(nx_), static_cast<float>(ny_)},
                   {x_, y_, static_cast<float>(nx_) * bin_, static_cast<float>(ny_) * bin_}, {0, 0}, 0.0f, WHITE);
}
