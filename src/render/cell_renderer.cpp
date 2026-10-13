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
constexpr Color kGoldenPlastid = {176, 140, 50, 210};   // fucoxantina (diatomeas)
constexpr Color kFrustule = {150, 128, 80, 220};
constexpr Color kColonyCell = {80, 150, 60, 210};
constexpr Color kChytrid = {150, 140, 110, 230};      // esporangio de quitridio (pared refringente)
constexpr Color kRhizoid = {120, 110, 90, 140};
constexpr Color kYeastWall = {150, 140, 110, 220};
constexpr Color kLorica = {120, 112, 84, 230};        // lórica del rotífero (cutícula)
constexpr Color kMastax = {90, 80, 60, 220};
constexpr Color kTentacle = {170, 140, 108, 200};
constexpr Color kNematocyst = {110, 90, 70, 220};
constexpr Color kNaupliarEye = {190, 50, 40, 230};
constexpr Color kAppendage = {120, 110, 84, 210};
constexpr float kRimMicrometers = 5.0f;
constexpr float kPi = 3.14159265f;

Vector2 toRay(Vec2 v) { return {v.x, v.y}; }
} // namespace

const char* stainName(Stain stain) {
    switch (stain) {
    case Stain::None: return "sin tincion";
    case Stain::CongoRed: return "rojo Congo (pH de las vacuolas)";
    case Stain::MethyleneBlue: return "azul de metileno (nucleos)";
    }
    return "";
}

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

// Dónde tiene cilios cada célula.
enum class CiliaPattern { All, Girdles, Crown, Posterior, Rostrum };

// Cilios: pelos cortos con onda metacrónica (la fase avanza a lo largo de la célula).
//  - All: toda la superficie (Paramecium); Girdles: dos cinturones (Didinium);
//  - Crown: corona del peristoma (Vorticella, Stentor); Posterior: telotroco.
void drawCilia(const BodyParticles& bp, const RingBody& body, float time, float px,
               CiliaPattern pattern = CiliaPattern::All, float lengthScale = 1.0f) {
    const float kLength = 7.0f * lengthScale; // cilios de Paramecium: ~10 µm
    constexpr float kBeatHz = 2.0f;   // visual: la real (~30 Hz) es un borrón al microscopio
    const float width = std::max(0.4f, px);
    float half = 0.0f;
    for (const Vec2& q : body.restRing)
        half = std::max(half, q.x);
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const float along = body.restRing[i].x / std::max(1e-3f, half);
        if (pattern == CiliaPattern::Girdles && std::abs(along - 0.55f) > 0.12f && std::abs(along + 0.1f) > 0.12f)
            continue;
        if (pattern == CiliaPattern::Crown && along < 0.75f)
            continue;
        if (pattern == CiliaPattern::Posterior && std::abs(along + 0.6f) > 0.12f)
            continue;
        if (pattern == CiliaPattern::Rostrum && along < -0.1f) // Trichonympha: mitad anterior
            continue;
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

// Marco del cuerpo: eje largo, lado, semilargo y semiancho de la forma de reposo.
struct Frame {
    Vec2 c, h, side;
    float half, width;
};
Frame frameOf(const RingBody& body) {
    Frame f{body.centroid, bodyHeading(body), {}, 0.0f, 0.0f};
    f.side = {-f.h.y, f.h.x};
    for (const Vec2& q : body.restRing) {
        f.half = std::max(f.half, std::abs(q.x));
        f.width = std::max(f.width, std::abs(q.y));
    }
    return f;
}

// Spirogyra: cloroplasto en cinta helicoidal (vista de costado: una onda que recorre la célula).
void drawSpiralPlastid(const RingBody& body, float time) {
    const Frame f = frameOf(body);
    Vec2 prev;
    constexpr int kSteps = 40;
    for (int k = 0; k <= kSteps; ++k) {
        const float t = -1.0f + 2.0f * static_cast<float>(k) / kSteps;
        const float wave = std::sin(3.0f * 3.14159265f * t + 0.05f * time);
        const Vec2 p = f.c + f.h * (0.92f * f.half * t) + f.side * (0.7f * f.width * wave);
        if (k > 0)
            DrawLineEx(toRay(prev), toRay(p), std::max(1.0f, 0.25f * f.width), kColonyCell);
        prev = p;
    }
}

// Diatomea pennada: frústula de sílice con rafe central y estrías.
void drawFrustule(const RingBody& body, float px) {
    const Frame f = frameOf(body);
    DrawLineEx(toRay(f.c - f.h * (0.85f * f.half)), toRay(f.c + f.h * (0.85f * f.half)), std::max(0.6f, px), kFrustule);
    for (int k = -6; k <= 6; ++k) {
        const Vec2 a = f.c + f.h * (0.13f * f.half * static_cast<float>(k));
        const float w = f.width * std::sqrt(std::max(0.0f, 1.0f - (0.13f * k) * (0.13f * k)));
        DrawLineEx(toRay(a + f.side * (0.25f * w)), toRay(a + f.side * (0.85f * w)), std::max(0.4f, 0.7f * px), kFrustule);
        DrawLineEx(toRay(a - f.side * (0.25f * w)), toRay(a - f.side * (0.85f * w)), std::max(0.4f, 0.7f * px), kFrustule);
    }
}

// Volvox: células biflageladas en la superficie de la esfera y colonias hijas adentro.
void drawVolvox(const BodyParticles& bp, const RingBody& body, float growth, int daughters, float time, float px) {
    const std::size_t n = body.ring.size();
    const float cell = 2.5f;
    for (std::size_t i = 0; i < n; i += 2) {
        const Vec2 p = ringPos(bp, body, i);
        const Vec2 o = outwardAt(bp, body, i);
        const Vec2 cpos = p - o * 4.0f;
        DrawCircleV(toRay(cpos), cell, kColonyCell);
        const float sw = 0.5f * std::sin(6.2831853f * 2.0f * time + static_cast<float>(i));
        DrawLineEx(toRay(cpos), toRay(cpos + rotate(o, std::cos(sw), std::sin(sw)) * 7.0f), std::max(0.3f, 0.6f * px), kFlagellum);
    }
    const Frame f = frameOf(body);
    for (int k = 0; k < daughters; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(daughters) + 0.4f;
        const Vec2 c = f.c + rotate(f.h, std::cos(a), std::sin(a)) * (0.45f * f.half);
        const float r = f.half * (0.06f + 0.16f * std::clamp(growth, 0.0f, 1.0f));
        DrawCircleV(toRay(c), r, Fade(kColonyCell, 0.5f));
        DrawRing(toRay(c), r - std::max(0.6f, px), r, 0, 360, 32, kColonyCell);
    }
}

// Pediastrum: cenobio plano de 16 células; las del borde llevan dos cuernos.
void drawPediastrum(const RingBody& body, float px) {
    const Frame f = frameOf(body);
    const float cell = 0.2f * f.half;
    for (int k = 0; k < 11; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / 11.0f;
        const Vec2 d = rotate(f.h, std::cos(a), std::sin(a));
        const Vec2 c = f.c + d * (0.72f * f.half);
        DrawCircleV(toRay(c), cell, kColonyCell);
        const Vec2 t = {-d.y, d.x};
        for (float s : {-1.0f, 1.0f})
            DrawLineEx(toRay(c + d * cell + t * (0.5f * cell * s)), toRay(c + d * (1.9f * cell) + t * (0.8f * cell * s)),
                       std::max(0.6f, px), kColonyCell);
    }
    for (int k = 0; k < 5; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / 5.0f + 0.3f;
        DrawCircleV(toRay(f.c + rotate(f.h, std::cos(a), std::sin(a)) * (0.33f * f.half)), cell, kColonyCell);
    }
}

// Quitridios: esporangios esféricos sobre la pared del alga, con rizoides hacia adentro.
void drawChytrids(const RingBody& body, const ChytridInfection& inf, float px) {
    const Frame f = frameOf(body);
    const float perSite = inf.mass / std::max<float>(1.0f, static_cast<float>(inf.siteAngle.size()));
    const float r = std::clamp(1.5f + 6.0f * std::cbrt(perSite), 1.5f, 9.0f);
    for (float a : inf.siteAngle) {
        const Vec2 d = rotate(f.h, std::cos(a), std::sin(a));
        // Punto de la membrana en esa dirección: el extremo del contorno de reposo.
        float reach = 0.0f;
        for (const Vec2& q : body.restRing) {
            const float len = length(q);
            if (len > 1e-3f && dot(q * (1.0f / len), Vec2{std::cos(a), std::sin(a)}) > 0.98f)
                reach = std::max(reach, len);
        }
        if (reach <= 0.0f)
            reach = f.width;
        const Vec2 wall = f.c + d * reach;
        for (int k = -1; k <= 1; ++k) // rizoides
            DrawLineEx(toRay(wall), toRay(wall - rotate(d, std::cos(0.5f * k), std::sin(0.5f * k)) * (0.6f * reach)),
                       std::max(0.4f, 0.6f * px), kRhizoid);
        DrawCircleV(toRay(wall + d * r), r, Fade(kChytrid, 0.6f));
        DrawRing(toRay(wall + d * r), r - std::max(0.5f, px), r, 0, 360, 24, kChytrid);
    }
}

// Elipse orientada según el cuerpo (DrawEllipse de raylib solo dibuja alineada a los ejes).
void drawOrientedEllipse(Vec2 c, Vec2 h, float a, float b, Color color) {
    constexpr int kSegments = 32;
    const Vec2 side = {-h.y, h.x};
    Vec2 prev = c + h * a;
    for (int k = 1; k <= kSegments; ++k) {
        const float t = 6.2831853f * static_cast<float>(k) / kSegments;
        const Vec2 p = c + h * (a * std::cos(t)) + side * (b * std::sin(t));
        // Los dos sentidos de giro: rlgl descarta triángulos según su orientación.
        DrawTriangle(toRay(c), toRay(prev), toRay(p), color);
        DrawTriangle(toRay(c), toRay(p), toRay(prev), color);
        prev = p;
    }
}

// Rotífero, tronco: lórica gruesa con espinas anteriores, mástax que mastica y estómago.
void drawRotiferTrunk(const BodyParticles& bp, const RingBody& body, float time, float px) {
    const Frame f = frameOf(body);
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        DrawLineEx(toRay(ringPos(bp, body, i)), toRay(ringPos(bp, body, i + 1)), std::max(1.8f, 2.2f * px), kLorica);
    for (int k = -3; k <= 3; ++k) { // espinas del borde anterior de la lórica
        if (k == 0)
            continue;
        const Vec2 b = f.c + f.h * (0.92f * f.half) + f.side * (0.16f * f.width * static_cast<float>(k));
        const Vec2 tip = b + f.h * (std::abs(k) == 3 ? 18.0f : 10.0f) + f.side * (2.0f * static_cast<float>(k));
        DrawLineEx(toRay(b), toRay(tip), std::max(1.2f, 1.5f * px), kLorica);
    }
    // Mástax: dos mandíbulas (trophi) que se abren y cierran ~2 veces por segundo.
    const Vec2 m = f.c + f.h * (0.45f * f.half);
    const float open = 0.35f + 0.3f * std::sin(6.2831853f * 2.0f * time);
    for (float s : {-1.0f, 1.0f}) {
        const Vec2 j = m + f.side * (s * 4.0f);
        const Vec2 d = rotate(f.h, std::cos(s * open), std::sin(s * open));
        DrawLineEx(toRay(j - d * 6.0f), toRay(j + d * 7.0f), std::max(2.0f, 2.5f * px), kMastax);
    }
    DrawCircleV(toRay(f.c - f.h * (0.15f * f.half)), 0.35f * f.width, Fade(kMastax, 0.25f)); // estómago
}

// Bdeloideo (Philodina): sin lórica, cuerpo en anillos telescópicos, dos ojos rojos y mástax.
void drawBdelloidTrunk(const RingBody& body, float time, float px) {
    const Frame f = frameOf(body);
    for (int k = -3; k <= 3; ++k) { // pliegues de la cutícula
        const Vec2 c = f.c + f.h * (0.25f * f.half * static_cast<float>(k));
        const float w = f.width * std::sqrt(std::max(0.0f, 1.0f - 0.0625f * static_cast<float>(k * k)));
        DrawLineEx(toRay(c - f.side * w), toRay(c + f.side * w), std::max(0.6f, px), Fade(kLorica, 0.5f));
    }
    for (float s : {-1.0f, 1.0f})
        DrawCircleV(toRay(f.c + f.h * (0.85f * f.half) + f.side * (0.25f * f.width * s)), std::max(2.0f, 2.5f * px), kNaupliarEye);
    const Vec2 m = f.c + f.h * (0.45f * f.half);
    const float open = 0.35f + 0.3f * std::sin(6.2831853f * 2.0f * time);
    for (float s : {-1.0f, 1.0f}) {
        const Vec2 d = rotate(f.h, std::cos(s * open), std::sin(s * open));
        DrawLineEx(toRay(m + f.side * (s * 3.0f) - d * 5.0f), toRay(m + f.side * (s * 3.0f) + d * 6.0f), std::max(2.0f, 2.5f * px), kMastax);
    }
}

// Rotífero, corona: dos ruedas de cilios que parecen girar (ondas metacrónicas).
void drawCorona(const BodyParticles& bp, const RingBody& body, Vec2 forward, float time, float px) {
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 b = ringPos(bp, body, i);
        if (dot(b - body.centroid, forward) < 0.0f)
            continue;
        const Vec2 o = outwardAt(bp, body, i);
        const float phase = 0.6f * static_cast<float>(i) - 6.2831853f * 3.0f * time;
        const float sw = 0.7f * std::sin(phase);
        DrawLineEx(toRay(b), toRay(b + rotate(o, std::cos(sw), std::sin(sw)) * 9.0f), std::max(0.5f, px), kCilia);
    }
}

// Rotífero, pie: dos dedos con glándulas adhesivas.
void drawFoot(const RingBody& body, Vec2 backward, float px) {
    const Frame f = frameOf(body);
    const Vec2 h = dot(f.h, backward) >= 0.0f ? f.h : f.h * -1.0f;
    const Vec2 tip = f.c + h * f.half;
    for (float s : {-1.0f, 1.0f})
        DrawLineEx(toRay(tip), toRay(tip + rotate(h, std::cos(0.35f * s), std::sin(0.35f * s)) * 12.0f),
                   std::max(1.5f, 2.0f * px), kLorica);
}

// Huevo de rotífero: cáscara y el embrión que se va formando.
void drawEgg(const RingBody& body, const Egg& egg, float px) {
    const Frame f = frameOf(body);
    const float hatch = std::clamp(1.0f - egg.hoursLeft / std::max(1.0f, speciesInfo(egg.species).eggHours), 0.0f, 1.0f);
    drawOrientedEllipse(f.c, f.h, 0.6f * f.half, 0.6f * f.width, Fade(kMastax, 0.15f + 0.3f * hatch));
    if (hatch > 0.6f) // ya se ve el mástax del embrión
        DrawCircleV(toRay(f.c + f.h * (0.2f * f.half)), std::max(1.5f, 2.0f * px), kMastax);
}

// Hydra: tentáculos con baterías de nematocistos; hipostoma (boca) en el polo anterior.
void drawHydra(const RingBody& body, const Tentacles& ten, float px) {
    const Frame f = frameOf(body);
    for (const Tentacle& arm : ten.arms) {
        for (std::size_t j = 0; j + 1 < arm.nodes.size(); ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(arm.nodes.size());
            DrawLineEx(toRay(arm.nodes[j]), toRay(arm.nodes[j + 1]), std::max(1.0f, 7.0f * (1.0f - 0.7f * t)), kTentacle);
            if (j % 2 == 1)
                DrawCircleV(toRay(arm.nodes[j]), std::max(1.0f, 2.2f * (1.0f - 0.5f * t)), kNematocyst);
        }
    }
    // Gastrodermis: la pared interna del tubo digestivo.
    drawOrientedEllipse(f.c, f.h, 0.8f * f.half, 0.45f * f.width, Fade(kMastax, 0.12f));
    DrawCircleV(toRay(f.c + f.h * (0.95f * f.half)), std::max(3.0f, 5.0f * px), kMouth);
}

// Nauplio: tres pares de apéndices articulados con setas y el ojo naupliar rojo.
void drawNauplius(const RingBody& body, const Locomotion* loco, float time, float px) {
    const Frame f = frameOf(body);
    const bool stroke = loco && loco->forward > 0.5f;
    for (int k = 0; k < 3; ++k) {
        const float along = 0.5f - 0.3f * static_cast<float>(k);
        const float len = (k == 0 ? 0.9f : 1.2f) * f.width;
        for (float s : {-1.0f, 1.0f}) {
            // Sale del borde del cuerpo; en el golpe barre hacia atrás, en la pausa queda abierto.
            const Vec2 base = f.c + f.h * (along * f.half) + f.side * (s * 0.92f * f.width);
            const float a0 = stroke ? s * (2.2f + 0.15f * std::sin(40.0f * time)) : s * (1.1f + 0.1f * static_cast<float>(k));
            Vec2 p = base;
            float a = a0;
            constexpr int kJoints = 4;
            for (int q = 0; q < kJoints; ++q) {
                const Vec2 d = rotate(f.h, std::cos(a), std::sin(a));
                const Vec2 next = p + d * (len / kJoints);
                DrawLineEx(toRay(p), toRay(next), std::max(1.2f, (3.5f - 0.7f * static_cast<float>(q)) * std::max(1.0f, px)), kAppendage);
                // Setas: plumosas, al final de cada artejo.
                DrawLineEx(toRay(next), toRay(next + rotate(d, std::cos(s * 0.9f), std::sin(s * 0.9f)) * 9.0f),
                           std::max(0.4f, 0.6f * px), kAppendage);
                p = next;
                a += s * 0.25f; // se curva hacia atrás
            }
        }
    }
    DrawCircleV(toRay(f.c + f.h * (0.75f * f.half)), std::max(3.0f, 0.1f * f.width), kNaupliarEye);
}

// Levadura en gemación: la yema crece en un polo hasta separarse.
void drawBud(const RingBody& body, float growth, float px) {
    const Frame f = frameOf(body);
    if (growth <= 0.05f)
        return;
    const float r = f.width * std::sqrt(std::clamp(growth, 0.0f, 1.0f));
    const Vec2 c = f.c + f.h * (f.half + 0.7f * r);
    DrawCircleV(toRay(c), r, {226, 220, 196, 200});
    DrawRing(toRay(c), r - std::max(0.4f, px), r, 0, 360, 24, kYeastWall);
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
            // Tallo: del punto de fijación al polo posterior; enrollado si el espasmonema se contrajo.
            if (const Anchored* anchor = reg.try_get<Anchored>(e)) {
                std::size_t back = 0;
                for (std::size_t i = 1; i < body.restRing.size(); ++i)
                    if (body.restRing[i].x < body.restRing[back].x)
                        back = i;
                const Vec2 base = ringPos(bp, body, back);
                const Vec2 hold = base + Vec2{minImage(anchor->holdfast.x - base.x, world.width()),
                                              minImage(anchor->holdfast.y - base.y, world.height())};
                const Vec2 along = base - hold;
                const Vec2 side = Vec2{-along.y, along.x} * (1.0f / std::max(1e-3f, length(along)));
                Vec2 prev = hold;
                constexpr int kTurns = 16;
                for (int k = 1; k <= kTurns; ++k) {
                    const float t = static_cast<float>(k) / kTurns;
                    const float coil = (k % 2 ? 1.0f : -1.0f) * 6.0f * anchor->contracted; // zigzag del resorte
                    const Vec2 p = hold + along * t + side * (k < kTurns ? coil : 0.0f);
                    DrawLineEx(toRay(prev), toRay(p), std::max(1.0f, 1.2f * px), kMembrane);
                    prev = p;
                }
                DrawCircleV(toRay(hold), std::max(2.0f, 2.0f * px), kMouth);
            }
            for (auto [pe, cap] : reg.view<Captured>().each()) { // tentáculo succionando a la presa
                if (cap.by != e || !reg.valid(pe))
                    continue;
                const RingBody& prey = reg.get<RingBody>(pe);
                const Vec2 pc = body.centroid + Vec2{minImage(prey.centroid.x - body.centroid.x, world.width()),
                                                     minImage(prey.centroid.y - body.centroid.y, world.height())};
                DrawLineEx(toRay(body.centroid), toRay(pc), std::max(1.5f, 2.0f * px), kMouth);
            }
            for (std::size_t i = 0; i < n; ++i)
                DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), membraneWidth, kMembrane);

            const Segment* segment = reg.try_get<Segment>(e);
            const Egg* egg = reg.try_get<Egg>(e);
            SpeciesId sid = species ? species->id : SpeciesId::Paramecium;
            if (segment && reg.valid(segment->organism) && reg.all_of<Species>(segment->organism))
                sid = reg.get<Species>(segment->organism).id;
            const bool metazoan = segment || egg || speciesInfo(sid).multicellular;
            if (egg) {
                drawEgg(body, *egg, px);
            } else if (segment) {
                // Corona (la primera parte) o pie, según de qué lado del tronco esté.
                const Compound* comp = reg.valid(segment->organism) ? reg.try_get<Compound>(segment->organism) : nullptr;
                const Vec2 trunk = reg.valid(segment->organism) && reg.all_of<RingBody>(segment->organism)
                                       ? reg.get<RingBody>(segment->organism).centroid
                                       : body.centroid;
                Vec2 away = body.centroid - trunk;
                away = away * (1.0f / std::max(1e-3f, length(away)));
                if (comp && !comp->parts.empty() && comp->parts[0] == e)
                    drawCorona(bp, body, away, time, px);
                else
                    drawFoot(body, away, px);
            } else if (sid == SpeciesId::Rotifer) {
                drawRotiferTrunk(bp, body, time, px);
            } else if (sid == SpeciesId::Philodina) {
                drawBdelloidTrunk(body, time, px);
            } else if (loco && sid == SpeciesId::Trichonympha) {
                drawCilia(bp, body, time, px, CiliaPattern::Rostrum, 2.4f); // miles de flagelos
                drawCilia(bp, body, time, px, CiliaPattern::Rostrum, 1.5f);
            } else if (sid == SpeciesId::Hydra) {
                if (const Tentacles* ten = reg.try_get<Tentacles>(e))
                    drawHydra(body, *ten, px);
            } else if (sid == SpeciesId::Nauplius) {
                drawNauplius(body, loco, time, px);
            } else if (cyst) { // quiste: pared gruesa y sin apéndices
                for (std::size_t i = 0; i < n; ++i)
                    DrawLineEx(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), std::max(2.5f, 3.0f * px), kCystWall);
            } else if (loco && sid == SpeciesId::Didinium) {
                drawCilia(bp, body, time, px, CiliaPattern::Girdles);
                // Probóscide: el cono del polo anterior con el que ataca.
                float half = 0.0f;
                for (const Vec2& q : body.restRing)
                    half = std::max(half, q.x);
                const Vec2 h = bodyHeading(body);
                const Vec2 tip = body.centroid + h * half;
                DrawTriangle(toRay(tip + h * 8.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), kMouth);
                DrawTriangle(toRay(tip + h * 8.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), kMouth);
            } else if (sid == SpeciesId::Spirogyra) {
                drawSpiralPlastid(body, time);
            } else if (sid == SpeciesId::Navicula) {
                drawFrustule(body, px);
            } else if (sid == SpeciesId::Volvox) {
                const Metabolism* vm = reg.try_get<Metabolism>(e);
                const float growth = vm && vm->birthBiomass > 0.0f ? vm->biomass / vm->birthBiomass - 1.0f : 0.0f;
                drawVolvox(bp, body, growth, speciesInfo(sid).colonyDaughters, time, px);
            } else if (sid == SpeciesId::Pediastrum) {
                drawPediastrum(body, px);
            } else if (loco && sid == SpeciesId::Vorticella) {
                drawCilia(bp, body, time, px, reg.all_of<Telotroch>(e) ? CiliaPattern::Posterior : CiliaPattern::Crown, 1.3f);
            } else if (loco && sid == SpeciesId::Stentor) {
                drawCilia(bp, body, time, px, CiliaPattern::Crown, 2.0f); // membranelas adorales
                drawCilia(bp, body, time, px, CiliaPattern::All, 0.4f);
            } else if (loco && sid == SpeciesId::Podophrya) {
                if (reg.all_of<Telotroch>(e)) {
                    drawCilia(bp, body, time, px, CiliaPattern::Posterior); // larva nadadora
                } else {
                    // Tentáculos capitados (con perilla en la punta) que irradian del cuerpo.
                    const float reach = speciesInfo(sid).tentacleReach;
                    for (int k = 0; k < 14; ++k) {
                        const float a = 6.2831853f * static_cast<float>(k) / 14.0f + 0.3f * std::sin(time + k);
                        const Vec2 d = {std::cos(a), std::sin(a)};
                        const Vec2 from = body.centroid + d * (0.8f * reach * 0.4f);
                        const Vec2 to = body.centroid + d * reach;
                        DrawLineEx(toRay(from), toRay(to), std::max(0.5f, px), kFlagellum);
                        DrawCircleV(toRay(to), std::max(1.2f, 1.5f * px), kMouth);
                    }
                }
            } else if (loco && sid == SpeciesId::Euplotes) {
                // Cirros: haces gruesos de cilios fusionados sobre los que "camina".
                for (std::size_t i = 0; i < n; i += std::max<std::size_t>(1, n / 10)) {
                    const Vec2 b = ringPos(bp, body, i);
                    const Vec2 o = outwardAt(bp, body, i);
                    const float sw = 0.4f * std::sin(6.2831853f * 3.0f * time + static_cast<float>(i));
                    const Vec2 d = rotate(o, std::cos(sw), std::sin(sw));
                    DrawLineEx(toRay(b), toRay(b + d * 9.0f), std::max(1.2f, 1.5f * px), kCilia);
                }
            } else if (loco && sid == SpeciesId::Lacrymaria) {
                drawCilia(bp, body, time, px, CiliaPattern::All, 0.6f);
                // Cuello extensible con la punta ciliada (toxicistos).
                if (const Neck* neck = reg.try_get<Neck>(e)) {
                    float half = 0.0f;
                    for (const Vec2& q : body.restRing)
                        half = std::max(half, q.x);
                    const Vec2 head = body.centroid + bodyHeading(body) * half;
                    const Vec2 dir = neck->length > 1.0f ? neck->direction : bodyHeading(body);
                    const Vec2 tip = head + dir * std::max(6.0f, neck->length);
                    DrawLineEx(toRay(head), toRay(tip), std::max(1.5f, 2.0f * px), kMembrane);
                    DrawCircleV(toRay(tip), std::max(2.5f, 2.0f * px), kMouth);
                }
            } else if (loco && sid == SpeciesId::Chlamydomonas) {
                drawTwinFlagella(body, *loco, time, px);
            } else if (loco && loco->type == LocomotionType::Cilia) {
                drawCilia(bp, body, time, px);
                // Citostoma: la boca al final del surco oral.
                const Vec2 m = ringPos(bp, body, loco->mouthRing);
                const Vec2 in = outwardAt(bp, body, loco->mouthRing) * -1.0f;
                DrawCircleV(toRay(m + in * 2.5f), 2.5f, kMouth);
            }
            if (!cyst && !metazoan && loco && loco->type == LocomotionType::Flagellum && sid != SpeciesId::Chlamydomonas &&
                sid != SpeciesId::Volvox && sid != SpeciesId::Trichonympha)
                drawFlagellum(bp, body, *loco, time, px);

            // Colonias (Volvox, Pediastrum): el interior es matriz gelatinosa, sin anatomía de una
            // sola célula.
            const bool colony = sid == SpeciesId::Volvox || sid == SpeciesId::Pediastrum || metazoan;
            // Gránulos y organelos del citoplasma, por tipo.
            for (std::size_t k = body.organelleCount; k < body.interior.size() && !colony; ++k) {
                const std::uint32_t s = body.interior[k];
                const float r = std::max(bp.radius[s], 0.75f * px);
                switch (static_cast<OrganelleTag>(bp.tag[s])) {
                case OrganelleTag::Mitochondrion: // bastoncitos
                    DrawRectanglePro({bp.x[s], bp.y[s], 2.6f * r, 1.2f * r}, {1.3f * r, 0.6f * r},
                                     static_cast<float>(s % 180), kMitochondrion);
                    break;
                case OrganelleTag::Chloroplast:
                    if (sid == SpeciesId::Spirogyra)
                        break; // su cloroplasto es la cinta helicoidal
                    DrawCircleV({bp.x[s], bp.y[s]}, r, sid == SpeciesId::Navicula ? kGoldenPlastid : kChloroplast);
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
                    Color fill = {v.r, v.g, v.b, alpha};
                    if (stain_ == Stain::CongoRed) {
                        // Fases de la vacuola de Paramecium: se acidifica (pH ~3, azul) en los
                        // primeros minutos, después los lisosomas la neutralizan (rojo).
                        const float done = 1.0f - fresh;
                        const float ph = v.open ? 7.0f : (done < 0.1f ? 7.0f - 40.0f * done : (done < 0.4f ? 3.0f : 3.0f + 7.0f * (done - 0.4f)));
                        const float blue = std::clamp((5.0f - ph) / 2.0f, 0.0f, 1.0f);
                        fill = {static_cast<unsigned char>(200 - 160 * blue), static_cast<unsigned char>(40 + 20 * blue),
                                static_cast<unsigned char>(60 + 110 * blue), 210};
                    }
                    DrawCircleV(c, r, fill);
                    DrawRing(c, std::max(0.0f, r - std::max(0.6f, px)), r, 0.0f, 360.0f, 32, kVacuoleRim);
                }
            }

            auto nucleus = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                const bool blue = stain_ == Stain::MethyleneBlue; // el ADN toma el colorante
                DrawCircleV(c, r, blue ? Color{40, 70, 170, 200} : kNucleus);
                DrawRing(c, r - std::max(0.8f, px), r, 0.0f, 360.0f, 40, blue ? Color{25, 45, 130, 230} : kNucleusRim);
                DrawCircleV({c.x + 0.2f * r, c.y - 0.15f * r}, 0.3f * r, kNucleolus);
            };
            auto contractile = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                DrawCircleV(c, r, kVacuole);
                DrawRing(c, r - std::max(0.6f, px), r, 0.0f, 360.0f, 32, kVacuoleRim);
            };
            if (!colony) {
                nucleus(anatomy.nucleus);
                contractile(anatomy.contractileVacuole);
            }
            if (const ChytridInfection* inf = reg.try_get<ChytridInfection>(e))
                drawChytrids(body, *inf, px);
            if (sid == SpeciesId::Candida)
                if (const Metabolism* ym = reg.try_get<Metabolism>(e); ym && ym->birthBiomass > 0.0f)
                    drawBud(body, ym->biomass / ym->birthBiomass - 1.0f, px);
            if (dividing && dividing->nucleus2 != ~0u) {
                nucleus(dividing->nucleus2);
                contractile(dividing->vacuole2);
            }
        });
    }
}
