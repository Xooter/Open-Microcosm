#include "render/cell_renderer.hpp"

#include "bio/cell.hpp"
#include "core/profiling.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <rlgl.h>

#include <algorithm>

namespace {
constexpr Color kMembrane = {70, 72, 60, 200};
constexpr Color kNucleus = {120, 128, 150, 95};
constexpr Color kNucleusRim = {80, 86, 105, 170};
constexpr Color kNucleolus = {70, 72, 95, 150};
constexpr Color kVacuole = {252, 252, 246, 140};
constexpr Color kVacuoleRim = {110, 120, 118, 170};
constexpr Color kGranule = {60, 62, 48, 170};
constexpr float kRimMicrometers = 5.0f;

Vector2 toRay(Vec2 v) { return {v.x, v.y}; }
} // namespace

CellRenderer::CellRenderer() {
    shader_ = LoadShader(nullptr, BIO_ASSET_DIR "/shaders/cell_brightfield.fs");
    shaderOk_ = shader_.id != 0 && shader_.id != rlGetShaderIdDefault();
    if (shaderOk_) {
        texelLoc_ = GetShaderLocation(shader_, "texelSize");
        rimLoc_ = GetShaderLocation(shader_, "rimPx");
    } else {
        TraceLog(LOG_WARNING, "bio: shader de células no disponible, uso relleno plano");
    }
}

CellRenderer::~CellRenderer() {
    if (target_.id != 0)
        UnloadRenderTexture(target_);
    if (shaderOk_)
        UnloadShader(shader_);
}

void CellRenderer::ensureTarget() {
    const int w = GetScreenWidth(), h = GetScreenHeight();
    if (target_.id != 0 && target_.texture.width == w && target_.texture.height == h)
        return;
    if (target_.id != 0)
        UnloadRenderTexture(target_);
    target_ = LoadRenderTexture(w, h);
    SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
}

// Dibuja la célula en cada copia del mundo toroidal donde se ve (cerca de los bordes).
template <typename Fn>
void CellRenderer::forEachImage(const World& world, const RingBody& body, Fn&& fn) {
    const float w = world.width(), h = world.height();
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            const Vec2 off = {ox * w, oy * h};
            if (body.aabbMax.x + off.x < 0 || body.aabbMin.x + off.x > w ||
                body.aabbMax.y + off.y < 0 || body.aabbMin.y + off.y > h)
                continue;
            rlPushMatrix();
            rlTranslatef(off.x, off.y, 0.0f);
            fn();
            rlPopMatrix();
        }
    }
}

void CellRenderer::fillCell(const RingBody& body, Color tint) {
    const Vec2* p = body.poly.data();
    const int n = static_cast<int>(body.poly.size());
    if (polygon::triangulate(p, n, triangles_, scratch_)) {
        for (std::size_t t = 0; t < triangles_.size(); t += 3)
            DrawTriangle(toRay(p[triangles_[t]]), toRay(p[triangles_[t + 1]]),
                         toRay(p[triangles_[t + 2]]), tint);
    } else {
        // Polígono degenerado: abanico desde el centroide (aproximado, pero no se rompe).
        for (int i = 0; i < n; ++i)
            DrawTriangle(toRay(body.centroid), toRay(p[i]), toRay(p[(i + 1) % n]), tint);
    }
}

void CellRenderer::renderCoverage(const World& world, const Camera2D& camera) {
    PROFILE_SCOPE("CellRenderer::coverage");
    ensureTarget();
    BeginTextureMode(target_);
    ClearBackground(BLANK);
    BeginMode2D(camera);
    rlDisableBackfaceCulling(); // los polígonos pueden venir en cualquier orientación
    for (const auto [e, body, look] : world.registry().view<RingBody, CellLook>().each()) {
        const Color tint = {look.r, look.g, look.b, 255};
        forEachImage(world, body, [&] { fillCell(body, tint); });
    }
    // raylib dibuja en lotes: hay que vaciar el lote ANTES de reactivar el culling, o los
    // triángulos se descartan al dibujarse más tarde.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndMode2D();
    EndTextureMode();
}

void CellRenderer::composite(const Camera2D& camera) {
    const auto w = static_cast<float>(target_.texture.width);
    const auto h = static_cast<float>(target_.texture.height);
    // Las render textures de OpenGL están invertidas en Y.
    const Rectangle src = {0, 0, w, -h};
    if (shaderOk_) {
        const float texel[2] = {1.0f / w, 1.0f / h};
        const float rim = std::clamp(kRimMicrometers * camera.zoom, 1.5f, 14.0f);
        SetShaderValue(shader_, texelLoc_, texel, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_, rimLoc_, &rim, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(shader_);
        DrawTextureRec(target_.texture, src, {0, 0}, WHITE);
        EndShaderMode();
    } else {
        DrawTextureRec(target_.texture, src, {0, 0}, Fade(WHITE, 0.3f));
    }
}

void CellRenderer::drawDetails(const World& world, const Camera2D& camera) {
    PROFILE_SCOPE("CellRenderer::details");
    const BodyParticles& bp = world.bodies();
    const float px = 1.0f / camera.zoom; // un píxel en µm
    const float membraneWidth = std::max(1.0f, 1.2f * px);

    for (const auto [e, body, anatomy] : world.registry().view<RingBody, CellAnatomy>().each()) {
        forEachImage(world, body, [&] {
            const std::size_t n = body.poly.size();
            for (std::size_t i = 0; i < n; ++i)
                DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), membraneWidth, kMembrane);

            for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k) {
                const std::uint32_t s = body.interior[k];
                const float d = std::max(2.0f * bp.radius[s], 1.5f * px);
                DrawRectangleV({bp.x[s] - 0.5f * d, bp.y[s] - 0.5f * d}, {d, d}, kGranule);
            }

            const Vector2 nuc = {bp.x[anatomy.nucleus], bp.y[anatomy.nucleus]};
            const float rn = bp.radius[anatomy.nucleus];
            DrawCircleV(nuc, rn, kNucleus);
            DrawRing(nuc, rn - std::max(0.8f, px), rn, 0.0f, 360.0f, 40, kNucleusRim);
            DrawCircleV({nuc.x + 0.2f * rn, nuc.y - 0.15f * rn}, 0.3f * rn, kNucleolus);

            const Vector2 vac = {bp.x[anatomy.contractileVacuole], bp.y[anatomy.contractileVacuole]};
            const float rv = bp.radius[anatomy.contractileVacuole];
            DrawCircleV(vac, rv, kVacuole);
            DrawRing(vac, rv - std::max(0.6f, px), rv, 0.0f, 360.0f, 32, kVacuoleRim);
        });
    }
}
