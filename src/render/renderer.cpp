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
    cells_.renderCoverage(world, camera);
    ClearBackground(kBackground);
    BeginMode2D(camera);

    overlay_.draw(world, overlay);

    // Todo se dibuja a tamaño real en µm, con un mínimo de ~1.5 px para que se vea al alejarse.
    const float minSize = 1.5f / camera.zoom;

    const AmbientParticles& a = world.ambient();
    for (std::size_t i = 0; i < a.size(); ++i) {
        const float d = std::max(2.0f * a.radius[i], minSize);
        const Color c = a.kind[i] == ParticleKind::Ink ? kInk : kDetritus;
        DrawRectangleV({a.x[i] - d * 0.5f, a.y[i] - d * 0.5f}, {d, d}, c);
    }

    // Bacterias como bastoncitos orientados (E. coli: ~2 × 1 µm).
    const Bacteria& b = world.bacteria();
    const float len = std::max(b.params.length, minSize);
    const float thick = std::max(b.params.radius * 2.0f, minSize * 0.6f);
    for (std::size_t i = 0; i < b.size(); ++i) {
        DrawRectanglePro({b.x[i], b.y[i], len, thick}, {len * 0.5f, thick * 0.5f},
                         b.heading[i] * RAD2DEG, kBacteria);
    }

    EndMode2D();

    cells_.composite(camera);

    BeginMode2D(camera);
    cells_.drawDetails(world, camera);
    DrawRectangleLinesEx({0, 0, world.width(), world.height()}, 1.5f / camera.zoom,
                         Fade(DARKGRAY, 0.5f));
    EndMode2D();
}
