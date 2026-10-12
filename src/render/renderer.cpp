#include "render/renderer.hpp"

#include "core/profiling.hpp"
#include "sim/world.hpp"

Renderer::Renderer() {
    for (int s = 0; s < kMaxSpecies; ++s)
        palette_[s] = ColorFromHSV(360.0f * s / kMaxSpecies, 0.75f, 1.0f);
}

void Renderer::drawWorld(const World& world, const Camera2D& camera) const {
    PROFILE_SCOPE("Renderer::drawWorld");
    const Particles& p = world.particles();

    BeginMode2D(camera);
    DrawRectangleLinesEx({0, 0, world.width(), world.height()}, 2.0f / camera.zoom, DARKGRAY);

    // raylib agrupa los quads en un único batch, así que miles de llamadas son baratas.
    // Si escalás a cientos de miles de partículas, el siguiente paso es un VBO propio
    // con instancing o point sprites (ver rlgl.h).
    const float size = 2.5f;
    for (std::size_t i = 0; i < p.size(); ++i)
        DrawRectangleV({p.x[i] - size * 0.5f, p.y[i] - size * 0.5f}, {size, size},
                       palette_[p.species[i]]);
    EndMode2D();
}
