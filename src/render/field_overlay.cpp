#include "render/field_overlay.hpp"

#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Viridis aproximado con 5 paradas: perceptualmente uniforme y legible para daltónicos.
Color colormap(float t) {
    static constexpr Color kStops[] = {
        {68, 1, 84, 255}, {59, 82, 139, 255}, {33, 145, 140, 255},
        {94, 201, 98, 255}, {253, 231, 37, 255}};
    t = std::clamp(t, 0.0f, 1.0f) * 4.0f;
    const int i = std::min(3, static_cast<int>(t));
    const float f = t - static_cast<float>(i);
    const Color a = kStops[i], b = kStops[i + 1];
    auto lerp = [f](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * f);
    };
    return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), 255};
}

} // namespace

const char* overlayName(Overlay overlay) {
    switch (overlay) {
    case Overlay::None: return "ninguno";
    case Overlay::Velocity: return "velocidad del agua";
    case Overlay::Oxygen: return "O2 disuelto";
    case Overlay::Nutrients: return "nutrientes (log)";
    case Overlay::Light: return "luz";
    }
    return "";
}

FieldOverlay::~FieldOverlay() {
    if (texture_.id != 0)
        UnloadTexture(texture_);
}

void FieldOverlay::ensureTexture(int width, int height) {
    if (texture_.id != 0 && texture_.width == width && texture_.height == height)
        return;
    if (texture_.id != 0)
        UnloadTexture(texture_);
    Image image = GenImageColor(width, height, BLANK);
    texture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    pixels_.resize(static_cast<std::size_t>(width) * height);
}

void FieldOverlay::draw(const World& world, Overlay overlay) {
    if (overlay == Overlay::None)
        return;
    PROFILE_SCOPE("FieldOverlay::draw");

    const Grid2D& ux = world.fluid().ux();
    const Grid2D& uy = world.fluid().uy();
    const std::size_t n = ux.v.size();
    ensureTexture(ux.nx, ux.ny);

    // Cada campo se normaliza a [0, 1] con una escala con sentido físico.
    std::vector<float> values(n); // overlay de debug: la asignación por frame es aceptable
    switch (overlay) {
    case Overlay::Velocity: {
        float maxSpeed = 1.0f; // piso de 1 µm/s para no amplificar ruido numérico
        for (std::size_t k = 0; k < n; ++k) {
            values[k] = std::hypot(ux.v[k], uy.v[k]);
            maxSpeed = std::max(maxSpeed, values[k]);
        }
        for (float& v : values)
            v /= maxSpeed;
        break;
    }
    case Overlay::Oxygen:
        for (std::size_t k = 0; k < n; ++k)
            values[k] = world.oxygen().grid().v[k] / units::kO2Saturation;
        break;
    case Overlay::Nutrients: {
        const auto& c = world.nutrients().grid().v;
        float maxLog = std::log1p(1.0f);
        for (std::size_t k = 0; k < n; ++k) {
            values[k] = std::log1p(std::max(0.0f, c[k]));
            maxLog = std::max(maxLog, values[k]);
        }
        for (float& v : values)
            v /= maxLog;
        break;
    }
    case Overlay::Light:
        values = world.light().v;
        break;
    case Overlay::None:
        break;
    }

    for (std::size_t k = 0; k < n; ++k)
        pixels_[k] = colormap(values[k]);
    UpdateTexture(texture_, pixels_.data());

    DrawTexturePro(texture_, {0, 0, static_cast<float>(ux.nx), static_cast<float>(ux.ny)},
                   {0, 0, world.width(), world.height()}, {0, 0}, 0.0f, Fade(WHITE, 0.6f));
}
