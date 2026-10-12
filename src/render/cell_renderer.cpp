#include "render/cell_renderer.hpp"

#include "bio/cell.hpp"
#include "bio/feeding.hpp"
#include "bio/locomotion.hpp"
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
constexpr Color kCilia = {70, 76, 62, 90};
constexpr Color kFlagellum = {60, 66, 52, 70};
constexpr Color kEyespot = {205, 80, 40, 220};  // estigma de Euglena (carotenoides)
constexpr Color kMouth = {60, 58, 48, 200};
constexpr float kRimMicrometers = 5.0f;
constexpr float kPi = 3.14159265f;

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

// Dibuja la célula en cada copia del mundo toroidal que cae dentro de la vista.
template <typename Fn>
void CellRenderer::forEachImage(const World& world, const RingBody& body, Rectangle view, Fn&& fn) {
    const float w = world.width(), h = world.height();
    constexpr float kMargin = 40.0f; // cilios y flagelos sobresalen de la caja
    for (int oy = -2; oy <= 2; ++oy) {
        for (int ox = -2; ox <= 2; ++ox) {
            const Vec2 off = {ox * w, oy * h};
            if (body.aabbMax.x + off.x + kMargin < view.x ||
                body.aabbMin.x + off.x - kMargin > view.x + view.width ||
                body.aabbMax.y + off.y + kMargin < view.y ||
                body.aabbMin.y + off.y - kMargin > view.y + view.height)
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

void CellRenderer::renderCoverage(const World& world, const Camera2D& camera, Rectangle view) {
    PROFILE_SCOPE("CellRenderer::coverage");
    ensureTarget();
    BeginTextureMode(target_);
    ClearBackground(BLANK);
    BeginMode2D(camera);
    rlDisableBackfaceCulling(); // los polígonos pueden venir en cualquier orientación
    for (const auto [e, body, look] : world.registry().view<RingBody, CellLook>().each()) {
        const Color tint = {look.r, look.g, look.b, 255};
        forEachImage(world, body, view, [&] { fillCell(body, tint); });
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

namespace {

Vec2 ringPos(const BodyParticles& bp, const RingBody& body, std::size_t i) {
    const std::uint32_t s = body.ring[i % body.ring.size()];
    return {bp.x[s], bp.y[s]};
}

Vec2 outwardAt(const BodyParticles& bp, const RingBody& body, std::size_t i) {
    const std::size_t n = body.ring.size();
    Vec2 t = ringPos(bp, body, i + 1) - ringPos(bp, body, i + n - 1);
    t = t * (1.0f / std::max(1e-4f, length(t)));
    return polygon::inwardNormal(t, body.restArea) * -1.0f;
}

// Cilios: pelos cortos con onda metacrónica (la fase avanza a lo largo de la célula).
void drawCilia(const BodyParticles& bp, const RingBody& body, float time, float px) {
    constexpr float kLength = 7.0f;   // cilios de Paramecium: ~10 µm
    constexpr float kBeatHz = 2.0f;   // visual: la real (~30 Hz) es un borrón al microscopio
    const float width = std::max(0.4f, px);
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const Vec2 base = ringPos(bp, body, i);
        const Vec2 o = outwardAt(bp, body, i);
        const float phase = 0.35f * static_cast<float>(i) - 2.0f * kPi * kBeatHz * time;
        const float swing = 0.6f * std::sin(phase);
        const Vec2 dir = rotate(o, std::cos(swing), std::sin(swing));
        DrawLineEx({base.x, base.y}, {base.x + dir.x * kLength, base.y + dir.y * kLength}, width, kCilia);
    }
}

// Flagelo anterior: onda sinusoidal. Batiendo a ~25 Hz se ve como un borrón: se dibujan
// tres fases superpuestas (desenfoque de movimiento, como en el microscopio).
void drawFlagellum(const BodyParticles& bp, const RingBody& body, const Locomotion& loco,
                   float time, float px) {
    const Vec2 h = bodyHeading(body);
    const Vec2 side = {-h.y, h.x};
    // Base: el punto de membrana más adelantado.
    std::size_t front = 0;
    float best = -1e9f;
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const float d = dot(ringPos(bp, body, i) - body.centroid, h);
        if (d > best) {
            best = d;
            front = i;
        }
    }
    const Vec2 base = ringPos(bp, body, front);
    float halfLength = 0.0f;
    for (const Vec2& q : body.restRing)
        halfLength = std::max(halfLength, std::abs(q.x));
    const float length = 2.2f * halfLength; // Euglena: flagelo de ~1 largo corporal
    const float beating = loco.speed > 0.0f ? 1.0f : 0.2f;
    const int copies = loco.speed > 0.0f ? 3 : 1;
    const float width = std::max(0.5f, px);
    for (int c = 0; c < copies; ++c) {
        const float phase0 = 2.0f * kPi * (25.0f * time + static_cast<float>(c) / 3.0f);
        Vec2 prev = base;
        constexpr int kSegments = 24;
        for (int k = 1; k <= kSegments; ++k) {
            const float t = static_cast<float>(k) / kSegments;
            const float amp = 4.0f * beating * t;
            const float wave = amp * std::sin(3.0f * kPi * t - phase0);
            const Vec2 p = base + h * (t * length) + side * wave;
            DrawLineEx({prev.x, prev.y}, {p.x, p.y}, width, kFlagellum);
            prev = p;
        }
    }
    // Mancha ocular (estigma) junto a la base del flagelo.
    const Vec2 eye = base - h * 4.0f + side * 2.0f;
    DrawCircleV({eye.x, eye.y}, 1.6f, kEyespot);
}

} // namespace

void CellRenderer::drawDetails(const World& world, const Camera2D& camera, Rectangle view) {
    PROFILE_SCOPE("CellRenderer::details");
    const BodyParticles& bp = world.bodies();
    const float px = 1.0f / camera.zoom; // un píxel en µm
    const float membraneWidth = std::max(1.0f, 1.2f * px);
    const float time = world.simTime();
    const entt::registry& reg = world.registry();

    for (const auto [e, body, anatomy] : reg.view<RingBody, CellAnatomy>().each()) {
        const Locomotion* loco = reg.try_get<Locomotion>(e);
        const Digestion* dig = reg.try_get<Digestion>(e);
        forEachImage(world, body, view, [&] {
            const std::size_t n = body.poly.size();
            for (std::size_t i = 0; i < n; ++i)
                DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), membraneWidth, kMembrane);

            if (loco && loco->type == LocomotionType::Cilia) {
                drawCilia(bp, body, time, px);
                // Citostoma: la boca al final del surco oral.
                const Vec2 m = ringPos(bp, body, loco->mouthRing);
                const Vec2 in = outwardAt(bp, body, loco->mouthRing) * -1.0f;
                DrawCircleV(toRay(m + in * 2.5f), 2.5f, kMouth);
            }
            if (loco && loco->type == LocomotionType::Flagellum)
                drawFlagellum(bp, body, *loco, time, px);

            for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k) {
                const std::uint32_t s = body.interior[k];
                const float d = std::max(2.0f * bp.radius[s], 1.5f * px);
                DrawRectangleV({bp.x[s] - 0.5f * d, bp.y[s] - 0.5f * d}, {d, d}, kGranule);
            }

            // Vacuolas digestivas: el color de la presa, que se aclara al digerirse.
            if (dig) {
                for (const FoodVacuole& v : dig->vacuoles) {
                    const Vector2 c = {bp.x[v.slot], bp.y[v.slot]};
                    const float r = bp.radius[v.slot];
                    const float fresh = v.initialPmol > 0.0f ? v.contentPmol / v.initialPmol : 0.0f;
                    const auto alpha = static_cast<unsigned char>(60.0f + 120.0f * fresh);
                    DrawCircleV(c, r, {v.r, v.g, v.b, alpha});
                    DrawRing(c, std::max(0.0f, r - std::max(0.6f, px)), r, 0.0f, 360.0f, 32, kVacuoleRim);
                }
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
