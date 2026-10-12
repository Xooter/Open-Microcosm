#include "render/cell_renderer.hpp"

#include "bio/cell.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "bio/organelles.hpp"
#include "bio/species.hpp"
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <cmath>
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
constexpr Color kMitochondrion = {120, 96, 70, 190};
constexpr Color kChloroplast = {70, 140, 55, 210};
constexpr Color kLysosome = {150, 78, 70, 200};
constexpr Color kTrichocyst = {90, 96, 80, 150};
constexpr Color kArmor = {150, 112, 62, 200};   // lórica/testa con hierro y manganeso
constexpr Color kDischarge = {80, 80, 70, 170};
constexpr Color kCystWall = {110, 96, 70, 220};
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
// `girdles`: solo dos cinturones (Didinium) en vez de toda la superficie.
void drawCilia(const BodyParticles& bp, const RingBody& body, float time, float px, bool girdles = false) {
    constexpr float kLength = 7.0f;   // cilios de Paramecium: ~10 µm
    constexpr float kBeatHz = 2.0f;   // visual: la real (~30 Hz) es un borrón al microscopio
    const float width = std::max(0.4f, px);
    float half = 0.0f;
    for (const Vec2& q : body.restRing)
        half = std::max(half, q.x);
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        if (girdles) {
            const float along = body.restRing[i].x / std::max(1e-3f, half);
            if (std::abs(along - 0.55f) > 0.12f && std::abs(along + 0.1f) > 0.12f)
                continue;
        }
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
// Chlamydomonas: dos flagelos anteriores que baten "en brazada" (puller).
void drawTwinFlagella(const RingBody& body, const Locomotion& loco, float time, float px) {
    const Vec2 h = bodyHeading(body);
    float half = 0.0f;
    for (const Vec2& q : body.restRing)
        half = std::max(half, q.x);
    const Vec2 base = body.centroid + h * half;
    const float length = 2.4f * half;
    const float stroke = loco.speed > 0.0f ? std::sin(6.2831853f * 5.0f * time) : 0.3f;
    for (float side : {-1.0f, 1.0f}) {
        Vec2 prev = base;
        constexpr int kSegments = 12;
        for (int k = 1; k <= kSegments; ++k) {
            const float t = static_cast<float>(k) / kSegments;
            const float angle = side * (0.6f + 0.9f * stroke * t); // los brazos se abren y se cierran
            const Vec2 dir = rotate(h, std::cos(angle), std::sin(angle));
            const Vec2 p = prev + dir * (length / kSegments);
            DrawLineEx({prev.x, prev.y}, {p.x, p.y}, std::max(0.4f, px), kFlagellum);
            prev = p;
        }
    }
    const Vec2 eye = base - h * (0.5f * half) + Vec2{-h.y, h.x} * (0.4f * half);
    DrawCircleV({eye.x, eye.y}, std::max(0.8f, 0.15f * half), kEyespot);
}

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
        const Dividing* dividing = reg.try_get<Dividing>(e);
        const Heredity* her = reg.try_get<Heredity>(e);
        const Species* species = reg.try_get<Species>(e);
        const bool cyst = reg.all_of<Cyst>(e);
        forEachImage(world, body, view, [&] {
            const std::size_t n = body.poly.size();
            for (std::size_t i = 0; i < n; ++i)
                DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), membraneWidth, kMembrane);

            const SpeciesId sid = species ? species->id : SpeciesId::Paramecium;
            if (cyst) { // quiste: pared gruesa y sin apéndices
                for (std::size_t i = 0; i < n; ++i)
                    DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), std::max(2.5f, 3.0f * px), kCystWall);
            } else if (loco && sid == SpeciesId::Didinium) {
                drawCilia(bp, body, time, px, true);
                // Probóscide: el cono del polo anterior con el que ataca.
                float half = 0.0f;
                for (const Vec2& q : body.restRing)
                    half = std::max(half, q.x);
                const Vec2 h = bodyHeading(body);
                const Vec2 tip = body.centroid + h * half;
                DrawTriangle(toRay(tip + h * 8.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), kMouth);
                DrawTriangle(toRay(tip + h * 8.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), kMouth);
            } else if (loco && sid == SpeciesId::Chlamydomonas) {
                drawTwinFlagella(body, *loco, time, px);
            } else if (loco && loco->type == LocomotionType::Cilia) {
                drawCilia(bp, body, time, px);
                // Citostoma: la boca al final del surco oral.
                const Vec2 m = ringPos(bp, body, loco->mouthRing);
                const Vec2 in = outwardAt(bp, body, loco->mouthRing) * -1.0f;
                DrawCircleV(toRay(m + in * 2.5f), 2.5f, kMouth);
            }
            if (!cyst && loco && loco->type == LocomotionType::Flagellum && sid != SpeciesId::Chlamydomonas)
                drawFlagellum(bp, body, *loco, time, px);

            // Gránulos y organelos del citoplasma, por tipo.
            for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k) {
                const std::uint32_t s = body.interior[k];
                const float r = std::max(bp.radius[s], 0.75f * px);
                switch (static_cast<OrganelleTag>(bp.tag[s])) {
                case OrganelleTag::Mitochondrion: // bastoncitos
                    DrawRectanglePro({bp.x[s], bp.y[s], 2.6f * r, 1.2f * r}, {1.3f * r, 0.6f * r},
                                     static_cast<float>(s % 180), kMitochondrion);
                    break;
                case OrganelleTag::Chloroplast:
                    DrawEllipse(static_cast<int>(bp.x[s]), static_cast<int>(bp.y[s]), 1.4f * r, r, kChloroplast);
                    DrawCircleV({bp.x[s], bp.y[s]}, r, kChloroplast);
                    break;
                case OrganelleTag::Lysosome:
                    DrawCircleV({bp.x[s], bp.y[s]}, r, kLysosome);
                    break;
                case OrganelleTag::Granule:
                    DrawRectangleV({bp.x[s] - r, bp.y[s] - r}, {2 * r, 2 * r}, kGranule);
                    break;
                }
            }

            if (her) {
                // Tricocistos: husos bajo la película, más densos con el gen.
                const float tri = her->genome[Gene::Trichocysts];
                if (tri > 0.05f) {
                    const std::size_t step = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(2.0f / tri)));
                    for (std::size_t i = 0; i < n; i += step) {
                        const Vec2 b = ringPos(bp, body, i);
                        const Vec2 in = outwardAt(bp, body, i) * -1.0f;
                        DrawLineEx(toRay(b + in * 1.5f), toRay(b + in * 4.5f), std::max(0.4f, px), kTrichocyst);
                    }
                }
                // Coraza: una capa externa gruesa (lórica o testa).
                if (her->traits.armor > 0.05f) {
                    const float t = 1.0f + 4.0f * her->traits.armor;
                    for (std::size_t i = 0; i < n; ++i) {
                        const Vec2 a = ringPos(bp, body, i) + outwardAt(bp, body, i) * (0.5f * t + 1.0f);
                        const Vec2 b = ringPos(bp, body, i + 1) + outwardAt(bp, body, i + 1) * (0.5f * t + 1.0f);
                        DrawLineEx(toRay(a), toRay(b), t, kArmor);
                    }
                }
            }
            // Tricocistos disparados: arpones largos hacia afuera.
            if (const Discharge* dis = reg.try_get<Discharge>(e)) {
                const float len = 25.0f * std::min(1.0f, dis->timer / 2.0f);
                for (std::size_t i = 0; i < n; i += 3) {
                    const Vec2 b = ringPos(bp, body, i);
                    DrawLineEx(toRay(b), toRay(b + outwardAt(bp, body, i) * len), std::max(0.4f, px), kDischarge);
                }
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

            auto nucleus = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                DrawCircleV(c, r, kNucleus);
                DrawRing(c, r - std::max(0.8f, px), r, 0.0f, 360.0f, 40, kNucleusRim);
                DrawCircleV({c.x + 0.2f * r, c.y - 0.15f * r}, 0.3f * r, kNucleolus);
            };
            auto contractile = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                DrawCircleV(c, r, kVacuole);
                DrawRing(c, r - std::max(0.6f, px), r, 0.0f, 360.0f, 32, kVacuoleRim);
            };
            nucleus(anatomy.nucleus);
            contractile(anatomy.contractileVacuole);
            if (dividing && dividing->nucleus2 != ~0u) {
                nucleus(dividing->nucleus2);
                contractile(dividing->vacuole2);
            }
        });
    }
}
