#include "render/renderer.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr Color kBackground = {236, 231, 214, 255}; // luz de lámpara halógena filtrada
constexpr Color kInk = {25, 30, 45, 170};
constexpr Color kDetritus = {120, 92, 55, 190};
constexpr Color kBacteria = {70, 80, 70, 150};
constexpr Color kViolacein = {96, 52, 140, 200};
constexpr Color kPhageInfected = {90, 96, 84, 110};   // infectada: algo más pálida antes de estallar
constexpr Color kBdelloplast = {84, 90, 76, 170};
constexpr Color kHypha = {150, 146, 128, 120};        // hifa hialina, casi transparente
constexpr Color kHyphaWall = {96, 92, 76, 170};
constexpr Color kSporangium = {120, 112, 86, 210};
constexpr Color kSpore = {90, 86, 70, 220};
} // namespace

Renderer::Renderer() {
    post_ = LoadShader(nullptr, BIO_ASSET_DIR "/shaders/microscope_post.fs");
    postOk_ = post_.id != 0 && post_.id != rlGetShaderIdDefault();
    if (postOk_) {
        resolutionLoc_ = GetShaderLocation(post_, "resolution");
        timeLoc_ = GetShaderLocation(post_, "time");
    }
}

Renderer::~Renderer() {
    if (scene_.id != 0)
        UnloadRenderTexture(scene_);
    if (postOk_)
        UnloadShader(post_);
}

void Renderer::ensureScene() {
    const int w = GetScreenWidth(), h = GetScreenHeight();
    if (scene_.id != 0 && scene_.texture.width == w && scene_.texture.height == h)
        return;
    if (scene_.id != 0)
        UnloadRenderTexture(scene_);
    scene_ = LoadRenderTexture(w, h);
    SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
}

void Renderer::drawWorld(const World& world, const Camera2D& camera, Overlay overlay) {
    PROFILE_SCOPE("Renderer::drawWorld");
    ensureScene();
    // Rectángulo visible en coordenadas del mundo [µm].
    const Vector2 a = GetScreenToWorld2D({0, 0}, camera);
    const Vector2 b = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()),
                                          static_cast<float>(GetScreenHeight())}, camera);
    const Rectangle view = {a.x, a.y, b.x - a.x, b.y - a.y};

    cells_.renderCoverage(world, camera, view);
    overlay_.update(world, overlay);
    // Todo el campo se dibuja en una textura y después pasa por la óptica del microscopio.
    BeginTextureMode(scene_);
    ClearBackground(kBackground);
    BeginMode2D(camera);

    // El mundo es un toro: se dibujan las copias que caen dentro de la vista.
    const float w = world.width(), h = world.height();
    const float minSize = 1.5f / camera.zoom; // tamaño real en µm, con un mínimo visible
    const AmbientParticles& amb = world.ambient();
    const Bacteria& bac = world.bacteria();
    const float len = std::max(bac.params.length, minSize);
    const float thick = std::max(bac.params.radius * 2.0f, minSize * 0.6f);
    // Bacterias: con instancing, un solo buffer por frame y una llamada por copia del mundo.
    const bool useInstancing = particles_.ready();
    if (useInstancing)
        particles_.upload(bac);
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
            // Cianobacterias filamentosas: cadenas de células verde-azuladas; los heterocistos,
            // más grandes, pálidos y de pared gruesa.
            for (const CyanoFilament& fil : world.cyanobacteria().filaments) {
                if (fil.x.empty())
                    continue;
                // La cadena es contigua: se lleva a la imagen de esta copia por su primera célula.
                const float fx = wrapCoord(fil.x[0], w) - fil.x[0], fy = wrapCoord(fil.y[0], h) - fil.y[0];
                if (!visible(fil.x[0] + fx, fil.y[0] + fy) && !visible(fil.x.back() + fx, fil.y.back() + fy))
                    continue;
                for (std::size_t k = 0; k < fil.x.size(); ++k) {
                    const Vector2 p = {fil.x[k] + fx + offX, fil.y[k] + fy + offY};
                    if (fil.heterocyst[k]) {
                        DrawCircleV(p, 3.0f, {214, 222, 180, 230});
                        DrawRing(p, 2.4f, 3.0f, 0, 360, 16, {120, 140, 100, 230});
                    } else {
                        DrawCircleV(p, std::max(2.2f, minSize), {64, 128, 112, 210});
                    }
                }
            }
            // Hongos: hifas como tubos hialinos (Saprolegnia, ~12 µm; Zoophagus, finas con
            // clavijas), zoosporangios en las puntas y zoosporas.
            for (const Mycelium& m : world.fungi().mycelia) {
                const float fx = wrapCoord(m.x[0], w) - m.x[0], fy = wrapCoord(m.y[0], h) - m.y[0];
                const bool thin = m.kind == FungusKind::Zoophagus;
                const float width = thin ? 4.0f : 11.0f;
                for (std::size_t k = 0; k < m.x.size(); ++k) {
                    const Vector2 p = {m.x[k] + fx + offX, m.y[k] + fy + offY};
                    if (!visible(m.x[k] + fx, m.y[k] + fy))
                        continue;
                    if (m.parent[k] >= 0) {
                        const auto pk = static_cast<std::size_t>(m.parent[k]);
                        const Vector2 q = {m.x[pk] + fx + offX, m.y[pk] + fy + offY};
                        const float wk = m.peg[k] ? 3.0f : width;
                        DrawLineEx(q, p, wk + 1.2f, kHyphaWall);
                        DrawLineEx(q, p, wk - 0.6f, kHypha);
                    }
                    if (m.peg[k])
                        DrawCircleV(p, 2.2f, kHyphaWall); // gota adhesiva
                    if (m.sporangium[k] > 0.0f) {
                        const float r = 0.5f * width + 6.0f * std::min(1.0f, m.sporangium[k]);
                        DrawCircleV(p, r, kSporangium);
                    }
                }
            }
            const Zoospores& zs = world.fungi().spores;
            for (std::size_t i = 0; i < zs.size(); ++i) {
                if (!visible(zs.x[i], zs.y[i]))
                    continue;
                const Vector2 p = {zs.x[i] + offX, zs.y[i] + offY};
                const float r = zs.kind[i] == FungusKind::Chytrid ? 1.6f : 2.0f;
                DrawCircleV(p, std::max(r, minSize), kSpore);
                if (zs.kind[i] != FungusKind::Zoophagus) // flagelo posterior (quitridio) o dos (oomiceto)
                    DrawLineEx(p, {p.x - std::cos(zs.heading[i]) * 7.0f, p.y - std::sin(zs.heading[i]) * 7.0f},
                               std::max(0.3f, 0.5f * minSize), kSpore);
            }
            // Bacterias como bastoncitos orientados (E. coli: ~2 × 1 µm). Las de C. violaceum en
            // quorum se tiñen de violeta (violaceína); el bdeloplasto es una esfera hinchada.
            if (useInstancing) {
                particles_.draw(offX, offY, len, thick);
            } else for (std::size_t i = 0; i < bac.size(); ++i) {
                if (!visible(bac.x[i], bac.y[i]))
                    continue;
                if (bac.bdello[i] >= 0.0f) {
                    DrawCircleV({bac.x[i] + offX, bac.y[i] + offY}, std::max(1.1f, minSize), kBdelloplast);
                    continue;
                }
                const Color c = bac.quorum[i] ? kViolacein : (bac.phage[i] >= 0.0f ? kPhageInfected : kBacteria);
                DrawRectanglePro({bac.x[i] + offX, bac.y[i] + offY, len, thick},
                                 {len * 0.5f, thick * 0.5f}, bac.heading[i] * RAD2DEG, c);
            }
            // Bdellovibrio: vibrios diminutos (~1 × 0,3 µm) que cruzan rapidísimo.
            const Bdellovibrios& bd = world.bdellovibrios();
            for (std::size_t i = 0; i < bd.size(); ++i) {
                if (!visible(bd.x[i], bd.y[i]))
                    continue;
                DrawRectanglePro({bd.x[i] + offX, bd.y[i] + offY, std::max(1.0f, minSize), std::max(0.4f, 0.5f * minSize)},
                                 {0.5f, 0.2f}, bd.heading[i] * RAD2DEG, kBacteria);
            }
        }
    }
    EndMode2D();

    cells_.composite(camera);

    BeginMode2D(camera);
    cells_.drawDetails(world, camera, view);
    // Lisis: la membrana revienta, el contenido se dispersa y el fantasma se desvanece.
    for (const LysisMark& m : world.lysisMarks()) {
        const float age = world.simTime() - m.time;
        const float fade = std::clamp(1.0f - age / 3.0f, 0.0f, 1.0f);
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox) {
                const Vector2 c = {m.at.x + ox * world.width(), m.at.y + oy * world.height()};
                if (c.x < view.x - m.radius * 3 || c.x > view.x + view.width + m.radius * 3 ||
                    c.y < view.y - m.radius * 3 || c.y > view.y + view.height + m.radius * 3)
                    continue;
                const float r = m.radius * (1.0f + 0.5f * age);
                DrawCircleV(c, r, {m.r, m.g, m.b, static_cast<unsigned char>(50 * fade)});
                DrawRing(c, r - 1.5f, r, 0, 360, 48, {90, 90, 70, static_cast<unsigned char>(120 * fade)});
                for (int k = 0; k < 24; ++k) { // gránulos que salen despedidos
                    const float a = 6.2831853f * static_cast<float>(k) / 24.0f + 0.37f * static_cast<float>(k % 5);
                    const float d = m.radius * (0.3f + 0.9f * age * (0.5f + 0.5f * std::sin(3.1f * k)));
                    DrawCircleV({c.x + std::cos(a) * d, c.y + std::sin(a) * d}, 0.8f, {60, 62, 48, static_cast<unsigned char>(170 * fade)});
                }
            }
    }
    EndMode2D();
    EndTextureMode();

    const auto sw = static_cast<float>(scene_.texture.width), sh = static_cast<float>(scene_.texture.height);
    const Rectangle src = {0, 0, sw, -sh}; // las render textures están invertidas en Y
    if (postOk_ && postEnabled_) {
        const float res[2] = {sw, sh};
        const float t = static_cast<float>(GetTime());
        SetShaderValue(post_, resolutionLoc_, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(post_, timeLoc_, &t, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(post_);
        DrawTextureRec(scene_.texture, src, {0, 0}, WHITE);
        EndShaderMode();
    } else {
        DrawTextureRec(scene_.texture, src, {0, 0}, WHITE);
    }
}
