#include "render/renderer.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr Color kBackground = {236, 231, 214, 255}; // luz de lámpara halógena filtrada
constexpr Color kInk = {25, 30, 45, 170};
constexpr Color kDetritus = {120, 92, 55, 190};
constexpr Color kBacteria = {70, 80, 70, 150};
} // namespace

void Renderer::drawWorld(const World& world, const Camera2D& camera, Overlay overlay) {
    PROFILE_SCOPE("Renderer::drawWorld");
    // Rectángulo visible en coordenadas del mundo [µm].
    const Vector2 a = GetScreenToWorld2D({0, 0}, camera);
    const Vector2 b = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()),
                                          static_cast<float>(GetScreenHeight())}, camera);
    const Rectangle view = {a.x, a.y, b.x - a.x, b.y - a.y};

    cells_.renderCoverage(world, camera, view);
    overlay_.update(world, overlay);
    ClearBackground(kBackground);
    BeginMode2D(camera);

    // El mundo es un toro: se dibujan las copias que caen dentro de la vista.
    const float w = world.width(), h = world.height();
    const float minSize = 1.5f / camera.zoom; // tamaño real en µm, con un mínimo visible
    const AmbientParticles& amb = world.ambient();
    const Bacteria& bac = world.bacteria();
    const float len = std::max(bac.params.length, minSize);
    const float thick = std::max(bac.params.radius * 2.0f, minSize * 0.6f);
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            const float offX = ox * w, offY = oy * h;
            const Rectangle copy = {offX, offY, w, h};
            if (!CheckCollisionRecs(copy, view))
                continue;
            overlay_.draw(world, offX, offY);
            const float x0 = view.x - offX - 5.0f, x1 = view.x + view.width - offX + 5.0f;
            const float y0 = view.y - offY - 5.0f, y1 = view.y + view.height - offY + 5.0f;
            auto visible = [&](float x, float y) { return x > x0 && x < x1 && y > y0 && y < y1; };

            for (std::size_t i = 0; i < amb.size(); ++i) {
                if (!visible(amb.x[i], amb.y[i]))
                    continue;
                const float d = std::max(2.0f * amb.radius[i], minSize);
                if (amb.kind[i] == ParticleKind::Ink)
                    DrawRectangleV({amb.x[i] + offX - d * 0.5f, amb.y[i] + offY - d * 0.5f}, {d, d}, kInk);
                else // detritos: grumos redondeados
                    DrawCircleV({amb.x[i] + offX, amb.y[i] + offY}, d * 0.5f, kDetritus);
            }
            // Bacterias como bastoncitos orientados (E. coli: ~2 × 1 µm).
            for (std::size_t i = 0; i < bac.size(); ++i) {
                if (!visible(bac.x[i], bac.y[i]))
                    continue;
                DrawRectanglePro({bac.x[i] + offX, bac.y[i] + offY, len, thick},
                                 {len * 0.5f, thick * 0.5f}, bac.heading[i] * RAD2DEG, kBacteria);
            }
        }
    }
    EndMode2D();

    cells_.composite(camera);

    BeginMode2D(camera);
    cells_.drawDetails(world, camera, view);
    EndMode2D();
}
