#include "render/cell_renderer.hpp"
#include "core/paths.hpp"
#include "render/shapes.hpp"

#include "bio/cell.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "bio/organelles.hpp"
#include "bio/species.hpp"
#include "core/profiling.hpp"
#include "game/world.hpp"
#include "render/structure_renderer.hpp"

#include <cmath>
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <rlgl.h>

#include <algorithm>
#include <vector>

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
constexpr Color kHydrogenosome = {104, 98, 86, 210};
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
constexpr Color kLorica = {120, 112, 84, 230};        // pared engrosada (capuchones de Oedogonium)
constexpr float kRimMicrometers = 5.0f;
constexpr float kPi = 3.14159265f;

Vector2 toRay(Vec2 v) { return {v.x, v.y}; }

Color dyeColor(Rgba c, float alpha) {
    auto b = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return {b(c.r), b(c.g), b(c.b), b(alpha)};
}

// El colorante encima del color natural: mezcla los canales y deja al menos la opacidad del colorante.
Color overColor(Color base, const StainResult& dye, float opacity = 0.9f) {
    const float t = std::clamp(dye.alpha, 0.0f, 1.0f);
    auto mixc = [&](unsigned char a, float b) { return static_cast<unsigned char>(a + (std::clamp(b, 0.0f, 1.0f) * 255.0f - a) * t); };
    Color o = {mixc(base.r, dye.color.r), mixc(base.g, dye.color.g), mixc(base.b, dye.color.b), base.a};
    o.a = std::max<unsigned char>(base.a, static_cast<unsigned char>(255.0f * opacity * t));
    return o;
}

// Cuánto almidón o glucógeno tiene la célula (0 a 1): sus reservas respecto de la biomasa.
float storageAmount(const Metabolism* m) {
    if (!m)
        return 0.5f;
    return std::clamp(m->reserves / std::max(1e-6f, 0.5f * m->biomass), 0.0f, 1.0f);
}

// Qué polisacárido guarda la célula y dónde se ve (en el cloroplasto el almidón; en gránulos el resto).
StainTarget storageTarget(Storage s) {
    switch (s) {
    case Storage::Starch: return StainTarget::Starch;
    case Storage::Glycogen: return StainTarget::Glycogen;
    case Storage::Paramylon: return StainTarget::Paramylon;
    default: return StainTarget::Count;
    }
}

// Color con que entra la célula entera en la textura de cobertura.
Color cellTint(const StainSpec* st, const entt::registry& reg, entt::entity e, const CellLook& look) {
    Color base = {look.r, look.g, look.b, 255};
    const Species* sp = reg.try_get<Species>(e);
    if (!st)
        return base;
    if (st->mode == StainMode::Fluorescence) {
        // Sin colorante la célula es invisible; la clorofila emite roja por sí sola.
        if (sp && speciesInfo(sp->id).chloroplasts) {
            const StainResult chl = evaluateStain(*st, StainTarget::Chlorophyll);
            if (chl.alpha > 0.0f) {
                // Las colonias huecas (Volvox) tienen la clorofila solo en una capa de células.
                const float k = (speciesInfo(sp->id).dryMassScale < 0.2f ? 0.35f : 1.0f) * std::min(1.0f, chl.alpha * 2.5f);
                return dyeColor({chl.color.r * k, chl.color.g * k, chl.color.b * k, 1.0f}, 1.0f);
            }
        }
        return {0, 0, 0, 255};
    }
    // Las levaduras tienen pared gruesa y se tiñen como grampositivas; el resto, con el colorante general.
    const SpeciesChemistry chem = sp ? speciesChemistry(sp->id) : SpeciesChemistry{};
    StainResult dye = evaluateStain(*st, StainTarget::Cytoplasm);
    if (chem.wall == CellWall::YeastGlucan) {
        const StainResult g = evaluateStain(*st, StainTarget::GramPositive);
        if (g.alpha > dye.alpha)
            dye = g;
    }
    const float chlorophyll = 0.0f;
    (void)chlorophyll;
    return overColor(base, dye, 1.0f);
}
// Mapa de limitación: el color dice qué frena el crecimiento (Liebig); lo que no tiene
// metabolismo propio (partes de un cuerpo, huevos, granos) queda con su color, atenuado.
Color limitationTint(const entt::registry& reg, entt::entity e, const CellLook& look) {
    const Metabolism* m = reg.try_get<Metabolism>(e);
    if (!m)
        return {static_cast<unsigned char>(look.r / 2 + 100), static_cast<unsigned char>(look.g / 2 + 100),
                static_cast<unsigned char>(look.b / 2 + 100), 255};
    const Color c = limitationColor(m->limiting);
    return {c.r, c.g, c.b, 255};
}
} // namespace

Color limitationColor(Limitation l) {
    switch (l) {
    case Limitation::None: return {70, 170, 80, 255};      // verde: crece a su tasa máxima
    case Limitation::Energy: return {230, 190, 40, 255};   // amarillo: comida o luz
    case Limitation::Phosphorus: return {200, 50, 160, 255};
    case Limitation::Nitrogen: return {40, 110, 220, 255};
    case Limitation::Silicon: return {40, 190, 200, 255};
    case Limitation::Iron: return {190, 90, 40, 255};
    case Limitation::Vitamin: return {140, 70, 220, 255};
    case Limitation::Starving: return {200, 40, 40, 255};  // rojo: autofagia
    case Limitation::Count: break;
    }
    return {128, 128, 128, 255};
}

CellRenderer::CellRenderer() {
    shader_ = LoadShader(nullptr, assetPath("shaders/cell_brightfield.fs").c_str());
    shaderOk_ = shader_.id != 0 && shader_.id != rlGetShaderIdDefault();
    if (shaderOk_) {
        texelLoc_ = GetShaderLocation(shader_, "texelSize");
        rimLoc_ = GetShaderLocation(shader_, "rimPx");
        fluorLoc_ = GetShaderLocation(shader_, "fluor");
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
    const int w = targetW_ > 0 ? targetW_ : GetScreenWidth(), h = targetH_ > 0 ? targetH_ : GetScreenHeight();
    if (target_.id != 0 && target_.texture.width == w && target_.texture.height == h)
        return;
    if (target_.id != 0)
        UnloadRenderTexture(target_);
    target_ = LoadRenderTexture(w, h);
    SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
}

// Dibuja la célula en cada copia del mundo toroidal que cae dentro de la vista (con placa,
// el mundo es cerrado y hay una sola).
template <typename Fn>
void CellRenderer::forEachImage(const World& world, const RingBody& body, Rectangle view, Fn&& fn) {
    const float w = world.width(), h = world.height();
    constexpr float kMargin = 40.0f; // cilios y flagelos sobresalen de la caja
    const int reach = world.dish().active() ? 0 : 2;
    for (int oy = -reach; oy <= reach; ++oy) {
        for (int ox = -reach; ox <= reach; ++ox) {
            const Vec2 off = {ox * w, oy * h};
            if (body.aabbMax.x + off.x + kMargin < view.x ||
                body.aabbMin.x + off.x - kMargin > view.x + view.width ||
                body.aabbMax.y + off.y + kMargin < view.y ||
                body.aabbMin.y + off.y - kMargin > view.y + view.height)
                continue;
            // Sin desplazamiento (siempre, con placa) no se toca la matriz: con una traslación
            // activa rlgl multiplica cada vértice por ella.
            if (ox == 0 && oy == 0) {
                fn();
                continue;
            }
            flushBatch(); // el lote no conoce la traslación de rlgl: lo de antes se dibuja sin ella
            rlPushMatrix();
            rlTranslatef(off.x, off.y, 0.0f);
            fn();
            flushBatch();
            rlPopMatrix();
        }
    }
}

const Vec2* CellRenderer::outline(const RingBody& body, float zoom, int& count) {
    count = static_cast<int>(body.poly.size());
    constexpr float kSegmentPx = 60.0f; // a partir de acá el polígono se ve facetado
    constexpr int kMaxSubdivisions = 16;
    constexpr float kMinSegmentPx = 3.0f; // más corto, la cuerda se aparta < 0,3 px del contorno
    float longest = 0.0f, perimeter = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float len = length(body.poly[static_cast<std::size_t>((i + 1) % count)] - body.poly[static_cast<std::size_t>(i)]);
        longest = std::max(longest, len);
        perimeter += len;
    }
    // Lejos (la placa entera), los puntos del anillo quedan a menos de un píxel entre sí: se toma 1
    // de cada `step`. La cuerda se aparta del contorno mucho menos de un píxel, y cobertura y
    // membrana usan el mismo contorno, así que siguen coincidiendo.
    const float meanPx = count > 0 ? perimeter * zoom / static_cast<float>(count) : 0.0f;
    if (count > 16 && meanPx > 0.0f && meanPx < kMinSegmentPx) {
        const int step = std::min(count / 8, static_cast<int>(kMinSegmentPx / meanPx));
        if (step >= 2) {
            smooth_.clear();
            for (int i = 0; i < count; i += step)
                smooth_.push_back(body.poly[static_cast<std::size_t>(i)]);
            count = static_cast<int>(smooth_.size());
            return smooth_.data();
        }
    }
    const int sub = std::min(kMaxSubdivisions, static_cast<int>(std::ceil(longest * zoom / kSegmentPx)));
    if (sub <= 1 || count < 4)
        return body.poly.data();
    smooth_.clear();
    const auto at = [&](int i) { return body.poly[static_cast<std::size_t>((i % count + count) % count)]; };
    for (int i = 0; i < count; ++i) {
        const Vec2 p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
        for (int k = 0; k < sub; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(sub), t2 = t * t, t3 = t2 * t;
            smooth_.push_back((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
                               (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) *
                              0.5f);
        }
    }
    count = static_cast<int>(smooth_.size());
    return smooth_.data();
}

void CellRenderer::fillCell(const RingBody& body, Color tint, float zoom) {
    int n = 0;
    const Vec2* p = outline(body, zoom, n);
    // Casi todas las células son estrelladas desde su centroide: entonces el abanico (todos los
    // triángulos con la misma orientación) cubre exactamente el polígono, en O(n) y sin ear
    // clipping (O(n²)) en cada cuadro.
    bool star = n >= 3;
    float sign = 0.0f;
    for (int i = 0; i < n && star; ++i) {
        const float c = cross(p[i] - body.centroid, p[(i + 1) % n] - body.centroid);
        if (sign == 0.0f)
            sign = c;
        star = c * sign > 0.0f;
    }
    if (star) {
        for (int i = 0; i < n; ++i)
            fastTriangle(toRay(body.centroid), toRay(p[i]), toRay(p[(i + 1) % n]), tint);
        return;
    }
    if (polygon::triangulate(p, n, triangles_, scratch_)) {
        for (std::size_t t = 0; t < triangles_.size(); t += 3)
            fastTriangle(toRay(p[triangles_[t]]), toRay(p[triangles_[t + 1]]),
                         toRay(p[triangles_[t + 2]]), tint);
    } else {
        // Polígono degenerado: abanico desde el centroide (aproximado, pero no se rompe).
        for (int i = 0; i < n; ++i)
            fastTriangle(toRay(body.centroid), toRay(p[i]), toRay(p[(i + 1) % n]), tint);
    }
}

void CellRenderer::renderCoverage(const World& world, const Camera2D& camera, Rectangle view) {
    PROFILE_SCOPE("CellRenderer::coverage");
    ensureTarget();
    BeginTextureMode(target_);
    ClearBackground(BLANK);
    BeginMode2D(camera);
    rlDisableBackfaceCulling(); // los polígonos pueden venir en cualquier orientación
    setActiveBatch(&batch_);
    for (const auto [e, body, look] : world.registry().view<RingBody, CellLook>().each()) {
        const Color tint = limitationMap_ ? limitationTint(world.registry(), e, look) : cellTint(stain_, world.registry(), e, look);
        forEachImage(world, body, view, [&] { fillCell(body, tint, camera.zoom); });
    }
    // raylib dibuja en lotes: hay que vaciar el lote ANTES de reactivar el culling, o los
    // triángulos se descartan al dibujarse más tarde (el nuestro también).
    flushBatch();
    setActiveBatch(nullptr);
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
        const float fluor = stain_ && stain_->mode == StainMode::Fluorescence ? 1.0f : 0.0f;
        SetShaderValue(shader_, fluorLoc_, &fluor, SHADER_UNIFORM_FLOAT);
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
        fastLine({base.x, base.y}, {base.x + dir.x * kLength, base.y + dir.y * kLength}, width, kCilia);
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
            fastLine({prev.x, prev.y}, {p.x, p.y}, std::max(0.4f, px), kFlagellum);
            prev = p;
        }
    }
    const Vec2 eye = base - h * (0.5f * half) + Vec2{-h.y, h.x} * (0.4f * half);
    fastCircleV({eye.x, eye.y}, std::max(0.8f, 0.15f * half), kEyespot);
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
            fastLine({prev.x, prev.y}, {p.x, p.y}, width, kFlagellum);
            prev = p;
        }
    }
    // Mancha ocular (estigma) junto a la base del flagelo.
    const Vec2 eye = base - h * 4.0f + side * 2.0f;
    fastCircleV({eye.x, eye.y}, 1.6f, kEyespot);
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
            fastLine(toRay(prev), toRay(p), std::max(1.0f, 0.25f * f.width), kColonyCell);
        prev = p;
    }
}

// Mougeotia: placa axial única. Con luz débil se ve de frente (llena el ancho de la célula); con
// luz fuerte gira de canto y se ve como una línea (en la realidad tarda minutos en girar).
void drawPlatePlastid(const RingBody& body, float light) {
    const Frame f = frameOf(body);
    const float edgeOn = std::clamp((light - 0.35f) / 0.5f, 0.0f, 1.0f);
    const float w = f.width * (0.8f - 0.65f * edgeOn * edgeOn * (3.0f - 2.0f * edgeOn));
    const Vec2 a = f.c - f.h * (0.88f * f.half), b = f.c + f.h * (0.88f * f.half);
    fastLine(toRay(a), toRay(b), std::max(1.0f, 2.0f * w), Fade(kChloroplast, 0.55f + 0.35f * edgeOn));
    for (int k = -2; k <= 2; ++k) // pirenoides en fila sobre la placa
        fastCircleV(toRay(f.c + f.h * (0.32f * f.half * static_cast<float>(k))), std::max(0.8f, std::min(0.3f * f.width, 2.2f)),
                    Fade(kColonyCell, 0.9f));
}

// Zygnema: dos cloroplastos estrellados con un pirenoide cada uno; el núcleo, entre ambos.
void drawStellatePlastids(const RingBody& body, float time) {
    const Frame f = frameOf(body);
    for (float s : {-1.0f, 1.0f}) {
        const Vec2 c = f.c + f.h * (0.48f * f.half * s);
        const float r = 0.55f * std::min(f.width, 0.5f * f.half);
        for (int k = 0; k < 9; ++k) { // brazos que irradian hasta la pared
            const float a = 6.2831853f * static_cast<float>(k) / 9.0f + 0.3f * s + 0.02f * time;
            const Vec2 d = f.h * std::cos(a) + f.side * std::sin(a);
            fastLine(toRay(c), toRay(c + d * (1.55f * r)), std::max(0.8f, 0.22f * r), kChloroplast);
        }
        fastCircleV(toRay(c), std::max(1.2f, 0.55f * r), kChloroplast);
        fastCircleV(toRay(c), std::max(0.8f, 0.28f * r), kColonyCell); // pirenoide
    }
}

// Oedogonium: cloroplasto parietal en red y capuchones (anillos de pared) junto a un polo, uno
// por cada división de esa célula.
void drawReticulatePlastid(const RingBody& body, int caps, float px) {
    const Frame f = frameOf(body);
    for (int i = -4; i <= 4; ++i)
        for (int j = -1; j <= 1; ++j) {
            const Vec2 p = f.c + f.h * (0.2f * f.half * static_cast<float>(i)) +
                           f.side * (0.45f * f.width * static_cast<float>(j) + 0.12f * f.width * static_cast<float>(i % 2));
            const Vec2 q = p + f.h * (0.2f * f.half) + f.side * (0.2f * f.width * static_cast<float>((i + j) % 2 ? 1 : -1));
            if (i < 4)
                fastLine(toRay(p), toRay(q), std::max(0.7f, 0.18f * f.width), Fade(kChloroplast, 0.8f));
            fastCircleV(toRay(p), std::max(0.6f, 0.12f * f.width), kChloroplast);
        }
    for (int k = 0; k < caps; ++k) {
        const Vec2 c = f.c + f.h * (f.half * (0.9f - 0.07f * static_cast<float>(k)));
        fastLine(toRay(c - f.side * (1.05f * f.width)), toRay(c + f.side * (1.05f * f.width)), std::max(0.6f, px), kLorica);
    }
}

// Diatomea pennada: frústula de sílice con rafe central y estrías.
void drawFrustule(const RingBody& body, float px) {
    const Frame f = frameOf(body);
    fastLine(toRay(f.c - f.h * (0.85f * f.half)), toRay(f.c + f.h * (0.85f * f.half)), std::max(0.6f, px), kFrustule);
    for (int k = -6; k <= 6; ++k) {
        const Vec2 a = f.c + f.h * (0.13f * f.half * static_cast<float>(k));
        const float w = f.width * std::sqrt(std::max(0.0f, 1.0f - (0.13f * k) * (0.13f * k)));
        fastLine(toRay(a + f.side * (0.25f * w)), toRay(a + f.side * (0.85f * w)), std::max(0.4f, 0.7f * px), kFrustule);
        fastLine(toRay(a - f.side * (0.25f * w)), toRay(a - f.side * (0.85f * w)), std::max(0.4f, 0.7f * px), kFrustule);
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
        fastCircleV(toRay(cpos), cell, kColonyCell);
        const float sw = 0.5f * std::sin(6.2831853f * 2.0f * time + static_cast<float>(i));
        fastLine(toRay(cpos), toRay(cpos + rotate(o, std::cos(sw), std::sin(sw)) * 7.0f), std::max(0.3f, 0.6f * px), kFlagellum);
    }
    const Frame f = frameOf(body);
    for (int k = 0; k < daughters; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(daughters) + 0.4f;
        const Vec2 c = f.c + rotate(f.h, std::cos(a), std::sin(a)) * (0.45f * f.half);
        const float r = f.half * (0.06f + 0.16f * std::clamp(growth, 0.0f, 1.0f));
        fastCircleV(toRay(c), r, Fade(kColonyCell, 0.5f));
        fastRing(toRay(c), r - std::max(0.6f, px), r, 0, 360, 32, kColonyCell);
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
        fastCircleV(toRay(c), cell, kColonyCell);
        const Vec2 t = {-d.y, d.x};
        for (float s : {-1.0f, 1.0f})
            fastLine(toRay(c + d * cell + t * (0.5f * cell * s)), toRay(c + d * (1.9f * cell) + t * (0.8f * cell * s)),
                       std::max(0.6f, px), kColonyCell);
    }
    for (int k = 0; k < 5; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / 5.0f + 0.3f;
        fastCircleV(toRay(f.c + rotate(f.h, std::cos(a), std::sin(a)) * (0.33f * f.half)), cell, kColonyCell);
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
            fastLine(toRay(wall), toRay(wall - rotate(d, std::cos(0.5f * k), std::sin(0.5f * k)) * (0.6f * reach)),
                       std::max(0.4f, 0.6f * px), kRhizoid);
        fastCircleV(toRay(wall + d * r), r, Fade(kChytrid, 0.6f));
        fastRing(toRay(wall + d * r), r - std::max(0.5f, px), r, 0, 360, 24, kChytrid);
    }
}

// Levadura en gemación: la yema crece en un polo hasta separarse.
void drawBud(const RingBody& body, float growth, float px) {
    const Frame f = frameOf(body);
    if (growth <= 0.05f)
        return;
    const float r = f.width * std::sqrt(std::clamp(growth, 0.0f, 1.0f));
    const Vec2 c = f.c + f.h * (f.half + 0.7f * r);
    fastCircleV(toRay(c), r, {226, 220, 196, 200});
    fastRing(toRay(c), r - std::max(0.4f, px), r, 0, 360, 24, kYeastWall);
}

} // namespace

void CellRenderer::drawDetails(const World& world, const Camera2D& camera, Rectangle view) {
    PROFILE_SCOPE("CellRenderer::details");
    const BodyParticles& bp = world.bodies();
    const float px = 1.0f / camera.zoom; // un píxel en µm
    // 1 µm (trazo de siempre) hasta 60 px; más cerca no engorda: la membrana real mide ~8 nm y en la
    // vista de modelo, de muy cerca, se dibuja con su espesor real (más 1 px para que se vea).
    const float membraneWidth = std::max(1.2f * px, lod_.view == ViewMode::Model && camera.zoom > 60.0f
                                                        ? 0.008f
                                                        : std::min(1.0f, 60.0f * px));
    StructureContext ctx;
    ctx.lod = &lod_;
    ctx.zoom = camera.zoom;
    ctx.time = world.simTime();
    ctx.view = view;
    ctx.particleBudget = lod_.maxParticlesPerFrame;
    const float time = world.simTime();
    const entt::registry& reg = world.registry();
    if (stain_ && stain_->mode == StainMode::Fluorescence) {
        drawFluorescence(world, camera, view);
        return;
    }
    setActiveBatch(&batch_); // membranas, gránulos y apéndices: una sola llamada (ver TriangleBatch)

    for (const auto [e, body, anatomy] : reg.view<RingBody, CellAnatomy>().each()) {
        const Locomotion* loco = reg.try_get<Locomotion>(e);
        const Digestion* dig = reg.try_get<Digestion>(e);
        const Dividing* dividing = reg.try_get<Dividing>(e);
        const Heredity* her = reg.try_get<Heredity>(e);
        const Species* species = reg.try_get<Species>(e);
        const bool cyst = reg.all_of<Cyst>(e);
        // Zoom semántico: una célula de pocos píxeles es una silueta con su membrana; sus
        // apéndices y núcleo aparecen con la morfología, y los organelos chicos solo de cerca.
        DetailLevel level =
            lod_.cellLevel(std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y), camera.zoom);
        if (e == inspection_ && lod_.enabled && lod_.inspectionBoost && level < DetailLevel::Subcellular)
            level = static_cast<DetailLevel>(static_cast<int>(level) + 1);
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
                    fastLine(toRay(prev), toRay(p), std::max(1.0f, 1.2f * px), kMembrane);
                    prev = p;
                }
                drawCircleFast(toRay(hold), std::max(2.0f, 2.0f * px), kMouth, px);
            }
            for (auto [pe, cap] : reg.view<Captured>().each()) { // tentáculo succionando a la presa
                if (cap.by != e || !reg.valid(pe))
                    continue;
                const RingBody& prey = reg.get<RingBody>(pe);
                const Vec2 pc = body.centroid + Vec2{minImage(prey.centroid.x - body.centroid.x, world.width()),
                                                     minImage(prey.centroid.y - body.centroid.y, world.height())};
                fastLine(toRay(body.centroid), toRay(pc), std::max(1.5f, 2.0f * px), kMouth);
            }
            {
                int m = 0;
                const Vec2* ring = outline(body, camera.zoom, m);
                for (int i = 0; i < m; ++i)
                    fastLine(toRay(ring[i]), toRay(ring[(i + 1) % m]), membraneWidth, kMembrane);
            }
            if (level < DetailLevel::Normal)
                return;

            const SpeciesId sid = species ? species->id : SpeciesId::Paramecium;
            if (loco && sid == SpeciesId::Trichonympha) {
                drawCilia(bp, body, time, px, CiliaPattern::Rostrum, 2.4f); // miles de flagelos
                drawCilia(bp, body, time, px, CiliaPattern::Rostrum, 1.5f);
            } else if (cyst) { // quiste: pared gruesa y sin apéndices
                for (std::size_t i = 0; i < n; ++i)
                    fastLine(toRay(body.poly[i]), toRay(body.poly[(i + 1) % n]), std::max(2.5f, 3.0f * px), kCystWall);
            } else if (loco && sid == SpeciesId::Didinium) {
                drawCilia(bp, body, time, px, CiliaPattern::Girdles);
                // Probóscide: el cono del polo anterior con el que ataca.
                float half = 0.0f;
                for (const Vec2& q : body.restRing)
                    half = std::max(half, q.x);
                const Vec2 h = bodyHeading(body);
                const Vec2 tip = body.centroid + h * half;
                fastTriangle(toRay(tip + h * 8.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), kMouth);
                fastTriangle(toRay(tip + h * 8.0f), toRay(tip - Vec2{-h.y, h.x} * 5.0f), toRay(tip + Vec2{-h.y, h.x} * 5.0f), kMouth);
            } else if (sid == SpeciesId::Spirogyra) {
                drawSpiralPlastid(body, time);
            } else if (sid == SpeciesId::Mougeotia) {
                const Metabolism* mm = reg.try_get<Metabolism>(e);
                drawPlatePlastid(body, mm ? mm->light : 0.5f);
            } else if (sid == SpeciesId::Zygnema) {
                drawStellatePlastids(body, time);
            } else if (sid == SpeciesId::Oedogonium && reg.all_of<Zoospore>(e)) {
                drawCilia(bp, body, time, px, CiliaPattern::Crown, 1.6f); // corona de flagelos
            } else if (sid == SpeciesId::Oedogonium) {
                const Lineage* lin = reg.try_get<Lineage>(e);
                drawReticulatePlastid(body, lin ? static_cast<int>(std::min<std::uint32_t>(4, lin->generation)) : 0, px);
            } else if (sid == SpeciesId::Navicula || sid == SpeciesId::Asterionella || sid == SpeciesId::Cyclotella) {
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
                        fastLine(toRay(from), toRay(to), std::max(0.5f, px), kFlagellum);
                        drawCircleFast(toRay(to), std::max(1.2f, 1.5f * px), kMouth, px);
                    }
                }
            } else if (loco && sid == SpeciesId::Euplotes) {
                // Cirros: haces gruesos de cilios fusionados sobre los que "camina".
                for (std::size_t i = 0; i < n; i += std::max<std::size_t>(1, n / 10)) {
                    const Vec2 b = ringPos(bp, body, i);
                    const Vec2 o = outwardAt(bp, body, i);
                    const float sw = 0.4f * std::sin(6.2831853f * 3.0f * time + static_cast<float>(i));
                    const Vec2 d = rotate(o, std::cos(sw), std::sin(sw));
                    fastLine(toRay(b), toRay(b + d * 9.0f), std::max(1.2f, 1.5f * px), kCilia);
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
                    fastLine(toRay(head), toRay(tip), std::max(1.5f, 2.0f * px), kMembrane);
                    drawCircleFast(toRay(tip), std::max(2.5f, 2.0f * px), kMouth, px);
                }
            } else if (loco && sid == SpeciesId::Chlamydomonas) {
                drawTwinFlagella(body, *loco, time, px);
            } else if (loco && loco->type == LocomotionType::Cilia) {
                drawCilia(bp, body, time, px);
                // Citostoma: la boca al final del surco oral.
                const Vec2 m = ringPos(bp, body, loco->mouthRing);
                const Vec2 in = outwardAt(bp, body, loco->mouthRing) * -1.0f;
                drawCircleFast(toRay(m + in * 2.5f), 2.5f, kMouth, px);
            }
            if (!cyst && loco && loco->type == LocomotionType::Flagellum && sid != SpeciesId::Chlamydomonas &&
                sid != SpeciesId::Volvox && sid != SpeciesId::Trichonympha && sid != SpeciesId::Oedogonium)
                drawFlagellum(bp, body, *loco, time, px);

            // Colonias (Volvox, Pediastrum): el interior es matriz gelatinosa, sin anatomía de una
            // sola célula.
            const bool colony = sid == SpeciesId::Volvox || sid == SpeciesId::Pediastrum;
            // Gránulos y organelos del citoplasma, por tipo.
            const bool organelles = level >= DetailLevel::Detail;
            const BodyPlan& plan = speciesPlan(sid);
            // Ribosomas libres (vista de modelo, de muy cerca): debajo de los organelos.
            if (organelles && !colony && lod_.view == ViewMode::Model && anatomy.nucleus != ~0u)
                if (const StructureSpec* rib = plan.find(StructureKind::Ribosomes); rib && ctx.particle(rib->width)) {
                    int m = 0;
                    const Vec2* ring = outline(body, camera.zoom, m);
                    drawCytoplasmicRibosomes(*rib, ring, m, body.centroid, bodyHeading(body),
                                             {bp.x[anatomy.nucleus], bp.y[anatomy.nucleus]}, bp.radius[anatomy.nucleus],
                                             static_cast<std::uint32_t>(entt::to_integral(e)), ctx);
                }
            // En los anaerobios sin mitocondrias (parabasálidos), el orgánulo energético simulado es
            // un hidrogenosoma: esférico, denso y sin cristas.
            const bool hydrogenosomes = plan.has(StructureKind::Hydrogenosome) && !plan.has(StructureKind::Mitochondrion);
            for (std::size_t k = body.organelleCount; k < body.interior.size() && !colony && organelles; ++k) {
                const std::uint32_t s = body.interior[k];
                const float r = std::max(bp.radius[s], 0.75f * px);
                // Reservas teñidas: el almidón envuelve al pirenoide del cloroplasto; el glucógeno y
                // el paramilón están en gránulos del citoplasma.
                if (stain_) {
                    const SpeciesChemistry chem = speciesChemistry(sid);
                    const StainTarget st = storageTarget(chem.storage);
                    const auto tag = static_cast<OrganelleTag>(bp.tag[s]);
                    const bool holder = chem.storage == Storage::Starch ? tag == OrganelleTag::Chloroplast : tag == OrganelleTag::Granule;
                    if (st != StainTarget::Count && holder && stainHasTarget(*stain_, st)) {
                        const float amount = storageAmount(reg.try_get<Metabolism>(e));
                        const StainResult dye = evaluateStain(*stain_, st, amount);
                        if (dye.alpha > 0.02f)
                            drawCircleFast({bp.x[s], bp.y[s]}, std::max(r * (1.4f + 1.2f * amount), 2.2f * px), dyeColor(dye.color, 0.4f + 0.6f * dye.alpha), px);
                    }
                }
                switch (static_cast<OrganelleTag>(bp.tag[s])) {
                case OrganelleTag::Mitochondrion: // bastoncitos
                    if (hydrogenosomes) {
                        drawCircleFast({bp.x[s], bp.y[s]}, r, kHydrogenosome, px);
                        drawRingFast({bp.x[s], bp.y[s]}, std::max(0.0f, r - std::max(0.15f * r, px)), r, 24, kVacuoleRim, px);
                        break;
                    }
                    fastRectPro({bp.x[s], bp.y[s], 2.6f * r, 1.2f * r}, {1.3f * r, 0.6f * r},
                                     static_cast<float>(s % 180), kMitochondrion);
                    break;
                case OrganelleTag::Chloroplast:
                    if (sid == SpeciesId::Spirogyra || sid == SpeciesId::Mougeotia || sid == SpeciesId::Zygnema ||
                        (sid == SpeciesId::Oedogonium && !reg.all_of<Zoospore>(e)))
                        break; // su cloroplasto se dibuja entero (cinta, placa, estrellas o red)
                    drawCircleFast({bp.x[s], bp.y[s]}, r, (sid == SpeciesId::Navicula || sid == SpeciesId::Asterionella || sid == SpeciesId::Cyclotella) ? kGoldenPlastid : kChloroplast, px);
                    break;
                case OrganelleTag::Lysosome: {
                    Color fill = kLysosome; // pH ~4,7: los lisosomas son ácidos
                    if (stain_ && stainHasTarget(*stain_, StainTarget::Acidic))
                        fill = overColor(fill, evaluateStain(*stain_, StainTarget::Acidic, 1.0f, 4.7f), 0.9f);
                    drawCircleFast({bp.x[s], bp.y[s]}, r, fill, px);
                    break;
                }
                case OrganelleTag::Granule:
                    fastRect({bp.x[s] - r, bp.y[s] - r}, {2 * r, 2 * r}, kGranule);
                    break;
                }
            }

            if (her) {
                // Tricocistos: husos bajo la película, más densos con el gen.
                const float tri = her->genome[Gene::Trichocysts];
                if (tri > 0.05f && organelles) {
                    const std::size_t step = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(2.0f / tri)));
                    for (std::size_t i = 0; i < n; i += step) {
                        const Vec2 b = ringPos(bp, body, i);
                        const Vec2 in = outwardAt(bp, body, i) * -1.0f;
                        fastLine(toRay(b + in * 1.5f), toRay(b + in * 4.5f), std::max(0.4f, px), kTrichocyst);
                    }
                }
                // Coraza: una capa externa gruesa (lórica o testa).
                if (her->traits.armor > 0.05f) {
                    const float t = 1.0f + 4.0f * her->traits.armor;
                    for (std::size_t i = 0; i < n; ++i) {
                        const Vec2 a = ringPos(bp, body, i) + outwardAt(bp, body, i) * (0.5f * t + 1.0f);
                        const Vec2 b = ringPos(bp, body, i + 1) + outwardAt(bp, body, i + 1) * (0.5f * t + 1.0f);
                        fastLine(toRay(a), toRay(b), t, kArmor);
                    }
                }
            }
            // Tricocistos disparados: arpones largos hacia afuera.
            if (const Discharge* dis = reg.try_get<Discharge>(e)) {
                const float len = 25.0f * std::min(1.0f, dis->timer / 2.0f);
                for (std::size_t i = 0; i < n; i += 3) {
                    const Vec2 b = ringPos(bp, body, i);
                    fastLine(toRay(b), toRay(b + outwardAt(bp, body, i) * len), std::max(0.4f, px), kDischarge);
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
                    if (stain_ && stainHasTarget(*stain_, StainTarget::Acidic)) {
                        // Fases de la vacuola de Paramecium: se acidifica (pH ~3) en los primeros
                        // minutos, después los lisosomas la neutralizan.
                        const float done = 1.0f - fresh;
                        const float ph = v.open ? 7.0f : (done < 0.1f ? 7.0f - 40.0f * done : (done < 0.4f ? 3.0f : 3.0f + 7.0f * (done - 0.4f)));
                        fill = overColor(fill, evaluateStain(*stain_, StainTarget::Acidic, 1.0f, ph), 0.85f);
                    }
                    drawCircleFast(c, r, fill, px);
                    drawRingFast(c, std::max(0.0f, r - std::max(0.6f, px)), r, 32, kVacuoleRim, px);
                }
            }

            auto nucleus = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                Color fillN = kNucleus, rimN = kNucleusRim; // el ADN toma el colorante
                if (stain_) {
                    const StainResult dye = evaluateStain(*stain_, StainTarget::Nucleus);
                    fillN = overColor(kNucleus, dye, 0.8f);
                    rimN = overColor(kNucleusRim, dye, 0.9f);
                    rimN.r = static_cast<unsigned char>(rimN.r * 0.7f);
                    rimN.g = static_cast<unsigned char>(rimN.g * 0.7f);
                    rimN.b = static_cast<unsigned char>(rimN.b * 0.7f);
                }
                drawCircleFast(c, r, fillN, px);
                drawRingFast(c, r - std::max(0.8f, px), r, 40, rimN, px);
                drawCircleFast({c.x + 0.2f * r, c.y - 0.15f * r}, 0.3f * r, kNucleolus, px);
            };
            auto contractile = [&](std::uint32_t slot) {
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                drawCircleFast(c, r, kVacuole, px);
                drawRingFast(c, r - std::max(0.6f, px), r, 32, kVacuoleRim, px);
            };
            if (!colony && level >= DetailLevel::Detail && anatomy.nucleus != ~0u)
                drawEndomembranes(speciesPlan(sid), {bp.x[anatomy.nucleus], bp.y[anatomy.nucleus]}, bp.radius[anatomy.nucleus],
                                  bodyHeading(body), static_cast<std::uint32_t>(entt::to_integral(e)), ctx);
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
    flushBatch();
    setActiveBatch(nullptr);
}

// Fluorescencia: sobre el fondo oscuro solo brillan los blancos teñidos (el ADN del núcleo, con un halo).
void CellRenderer::drawFluorescence(const World& world, const Camera2D&, Rectangle view) {
    const BodyParticles& bp = world.bodies();
    const entt::registry& reg = world.registry();
    const StainResult dye = evaluateStain(*stain_, StainTarget::Nucleus);
    if (dye.alpha <= 0.0f)
        return;
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto [e, body, anatomy] : reg.view<RingBody, CellAnatomy>().each()) {
        const Species* species = reg.try_get<Species>(e);
        const SpeciesId sid = species ? species->id : SpeciesId::Paramecium;
        if (sid == SpeciesId::Volvox || sid == SpeciesId::Pediastrum)
            continue;
        const Dividing* dividing = reg.try_get<Dividing>(e);
        forEachImage(world, body, view, [&] {
            auto nucleus = [&](std::uint32_t slot) {
                if (slot == ~0u)
                    return;
                const Vector2 c = {bp.x[slot], bp.y[slot]};
                const float r = bp.radius[slot];
                fastCircleV(c, r * 2.1f, dyeColor(dye.color, 0.10f * dye.alpha));
                fastCircleV(c, r * 1.5f, dyeColor(dye.color, 0.22f * dye.alpha));
                fastCircleV(c, r, dyeColor(dye.color, 0.95f * dye.alpha));
            };
            nucleus(anatomy.nucleus);
            if (dividing)
                nucleus(dividing->nucleus2);
        });
    }
    rlDrawRenderBatchActive();
    EndBlendMode();
}
