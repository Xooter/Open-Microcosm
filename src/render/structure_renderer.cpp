#include "render/structure_renderer.hpp"

#include "core/profiling.hpp"
#include "sim/polygon.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTau = 2.0f * kPi;

// Colores de la vista de modelo: esquemáticos y apagados, como un dibujo de libro de texto (no son
// los de la luz del microscopio).
constexpr Color kMembraneLine = {38, 46, 42, 230};
constexpr Color kWallLine = {120, 130, 80, 210};
constexpr Color kNucleoid = {214, 214, 176, 150};
constexpr Color kRibosome = {70, 64, 58, 200};
constexpr Color kFlagellin = {58, 64, 58, 210};
constexpr Color kPilus = {72, 80, 66, 150};
constexpr Color kCapsidFull = {58, 60, 72, 235};
constexpr Color kCapsidEmpty = {196, 198, 210, 150};
constexpr Color kPhageLine = {44, 46, 56, 235};
constexpr Color kDna = {150, 160, 200, 200};
constexpr Color kBdello = {84, 96, 80, 220};
constexpr Color kEr = {78, 96, 122, 200};
constexpr Color kGolgi = {124, 98, 70, 210};

// El flagelo gira a ~100 Hz: a 60 cuadros por segundo eso se vería como un parpadeo (aliasing).
// Se muestra la rotación ralentizada; es una decisión de visualización, no de la biología.
constexpr float kFlagellumVisualHz = 6.0f;
// Período del fago T4 (a 37 °C): eclipse ~12 de ~25 min de latencia (Hyman & Abedon 2009). Antes
// del eclipse no hay viriones maduros adentro; después se ensamblan hasta el estallido.
constexpr float kPhageEclipse = 0.45f;
// T4 degrada el ADN del huésped en los primeros minutos de la infección.
constexpr float kHostDnaDegraded = 0.2f;

float hash01(std::uint32_t a, std::uint32_t b) {
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

Vector2 ray(Vec2 v) { return {v.x, v.y}; }

// Punto del contorno de la cápsula (semilargo `halfL`, radio `r`) en la fracción u del perímetro,
// con su normal hacia afuera.
void capsulePoint(float halfL, float r, float u, Vec2& p, Vec2& n) {
    const float straight = 2.0f * std::max(0.0f, halfL - r), arc = kPi * r;
    const float s = u * 2.0f * (straight * 0.5f + arc);
    const float flat = std::max(0.0f, halfL - r);
    if (s < straight * 0.5f) { // lado de arriba (y = −r), de −flat a +flat
        p = {-flat + 2.0f * flat * (s / std::max(1e-6f, straight * 0.5f)), -r};
        n = {0.0f, -1.0f};
    } else if (s < straight * 0.5f + arc) { // polo +x
        const float a = -0.5f * kPi + (s - straight * 0.5f) / std::max(1e-6f, r);
        n = {std::cos(a), std::sin(a)};
        p = Vec2{flat, 0.0f} + n * r;
    } else if (s < straight + arc) { // lado de abajo (y = +r), de +flat a −flat
        p = {flat - 2.0f * flat * ((s - straight * 0.5f - arc) / std::max(1e-6f, straight * 0.5f)), r};
        n = {0.0f, 1.0f};
    } else { // polo −x
        const float a = 0.5f * kPi + (s - straight - arc) / std::max(1e-6f, r);
        n = {std::cos(a), std::sin(a)};
        p = Vec2{-flat, 0.0f} + n * r;
    }
}

// Contorno de cápsula de espesor `thick` cuyo borde externo queda `inset` hacia adentro.
void capsuleOutline(float halfL, float r, float inset, float thick, Color c) {
    const float d = inset + thick;
    if (r - d <= 0.0f)
        return;
    DrawRectangleRoundedLinesEx({-halfL + d, -r + d, 2.0f * (halfL - d), 2.0f * (r - d)}, 1.0f, 24, thick, c);
}

// Hélice plana (proyección) desde `base` en la dirección `dir`.
void drawHelix(Vec2 base, Vec2 dir, float length, float width, float pitch, float amplitude, float phase, Color c) {
    const Vec2 side = {-dir.y, dir.x};
    constexpr int kSegments = 48;
    Vec2 prev = base;
    for (int k = 1; k <= kSegments; ++k) {
        const float s = length * static_cast<float>(k) / kSegments;
        const float grow = std::min(1.0f, s / (0.5f * pitch)); // sale recto del gancho
        const Vec2 p = base + dir * s + side * (amplitude * grow * std::sin(kTau * s / pitch + phase));
        DrawLineEx(ray(prev), ray(p), width, c);
        prev = p;
    }
}

bool inView(const StructureContext& ctx, Vec2 origin, float c, float s, Vec2 local) {
    const float x = origin.x + c * local.x - s * local.y, y = origin.y + s * local.x + c * local.y;
    return x >= ctx.view.x && x <= ctx.view.x + ctx.view.width && y >= ctx.view.y && y <= ctx.view.y + ctx.view.height;
}

void drawFlagella(const BacteriumState& b, const StructureSpec& spec, float halfL, float r, StructureContext& ctx) {
    const float phase = kTau * kFlagellumVisualHz * ctx.time;
    // Forma normal (al nadar) y rizada (al girar), con su paso y diámetro de hélice reales
    // (Turner, Ryu & Berg 2000): ~2,3 µm × 0,4 µm y ~1,1 µm × 0,5 µm.
    const float pitch = b.tumbling ? 1.1f : 2.3f, amp = b.tumbling ? 0.25f : 0.2f;
    if (spec.arrangement == Arrangement::Polar) { // un flagelo en el polo trasero
        const Vec2 dir = b.tumbling ? Vec2{-0.94f, 0.34f} : Vec2{-1.0f, 0.0f}; // al girar, golpea de costado
        drawHelix({-halfL, 0.0f}, dir, spec.length, spec.width, pitch, amp, phase, kFlagellin);
        return;
    }
    // Perítricos: al nadar, los motores giran en sentido antihorario y los flagelos forman un haz
    // detrás de la célula; al girar (tumble), alguno cambia de sentido y el haz se deshace.
    const Vec2 bundle = {-halfL - 0.3f, 0.0f};
    for (int k = 0; k < spec.count; ++k) {
        Vec2 p, n;
        capsulePoint(halfL, r, hash01(b.id, 100u + static_cast<std::uint32_t>(k)), p, n);
        if (b.tumbling) {
            const Vec2 dir = n * 0.8f + Vec2{-0.6f, 0.0f};
            drawHelix(p, dir * (1.0f / std::max(1e-3f, length(dir))), spec.length, spec.width, pitch, amp,
                      phase + static_cast<float>(k), kFlagellin);
        } else {
            DrawLineEx(ray(p), ray(bundle), spec.width, kFlagellin); // del gancho al haz
            const Vec2 off = {0.0f, 0.05f * (hash01(b.id, 200u + static_cast<std::uint32_t>(k)) - 0.5f)};
            drawHelix(bundle + off, {-1.0f, 0.0f}, spec.length, spec.width, pitch, amp,
                      phase + 0.2f * static_cast<float>(k), kFlagellin);
        }
    }
}

// Bdellovibrio creciendo como filamento en el periplasma del bdeloplasto: se alarga hasta medir
// lo de sus `progeny` hijas y al final se tabica en todas a la vez (Fenton et al. 2010).
void drawBdelloInside(const BacteriumState& b, float r) {
    const float f = std::clamp(b.bdelloFraction, 0.0f, 1.0f);
    const float len = 1.0f + (static_cast<float>(std::max(1, b.bdelloProgeny)) - 1.0f) * f; // [µm]
    const float radius = 0.72f * r, width = 0.25f;
    const float a0 = kTau * hash01(b.id, 7u);
    constexpr int kSegments = 64;
    Vec2 prev{};
    for (int k = 0; k <= kSegments; ++k) {
        const float s = len * static_cast<float>(k) / kSegments;
        const float rr = radius * (1.0f - 0.12f * s / (kTau * radius)); // se enrolla hacia adentro
        const float a = a0 + s / radius;
        const Vec2 p = {std::cos(a) * rr, std::sin(a) * rr};
        if (k > 0)
            DrawLineEx(ray(prev), ray(p), width, kBdello);
        prev = p;
    }
    if (f > 0.85f) // tabiques entre las hijas
        for (int k = 1; k < b.bdelloProgeny; ++k) {
            const float s = len * static_cast<float>(k) / static_cast<float>(b.bdelloProgeny);
            const float rr = radius * (1.0f - 0.12f * s / (kTau * radius));
            const float a = a0 + s / radius;
            const Vec2 d = {std::cos(a), std::sin(a)};
            DrawLineEx(ray(d * (rr - 0.5f * width)), ray(d * (rr + 0.5f * width)), 0.03f, kMembraneLine);
        }
}

} // namespace

void drawBacteriumStructures(const BacteriumState& b, const BodyPlan& plan, StructureContext& ctx) {
    PROFILE_SCOPE("drawBacteriumStructures");
    const float c = std::cos(b.heading), s = std::sin(b.heading);
    const bool bdelloplast = b.bdelloFraction >= 0.0f;
    // El bdeloplasto es la presa redondeada (como en el bastoncito instanciado: 0,6 del semilargo).
    const float halfL = bdelloplast ? 0.3f * b.length : 0.5f * b.length;
    const float r = bdelloplast ? halfL : 0.5f * b.width;
    rlPushMatrix();
    rlTranslatef(b.at.x, b.at.y, 0.0f);
    rlRotatef(b.heading * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // Apéndices (debajo del cuerpo).
    if (const StructureSpec* fl = plan.find(StructureKind::Flagellum); fl && !bdelloplast && ctx.allowed(*fl) && ctx.line(fl->width))
        drawFlagella(b, *fl, halfL, r, ctx);
    if (const StructureSpec* pi = plan.find(StructureKind::Pili); pi && !bdelloplast && ctx.allowed(*pi) && ctx.line(pi->width))
        for (int k = 0; k < pi->count; ++k) {
            Vec2 p, n;
            capsulePoint(halfL, r, hash01(b.id, 1000u + static_cast<std::uint32_t>(k)), p, n);
            const float len = pi->length * (0.5f + hash01(b.id, 3000u + static_cast<std::uint32_t>(k)));
            DrawLineEx(ray(p), ray(p + n * len), pi->width, kPilus);
        }

    // Citoplasma: la misma cápsula y color que el nivel anterior (no cambia la identidad visual).
    DrawRectangleRounded({-halfL, -r, 2.0f * halfL, 2.0f * r}, 1.0f, 24, b.body);

    // Envoltura: capas de afuera hacia adentro con su espesor real. Si ninguna capa alcanza a verse
    // sola, se dibuja la envoltura entera como un solo contorno.
    float depth = 0.0f;
    bool anyLayer = false;
    if (ctx.model()) {
        for (const StructureSpec& st : plan.structures) {
            if (st.arrangement != Arrangement::Envelope)
                continue;
            if (ctx.line(st.width)) {
                capsuleOutline(halfL, r, depth, st.width, st.kind == StructureKind::Peptidoglycan ? kWallLine : kMembraneLine);
                anyLayer = true;
            }
            depth += st.width + (st.kind == StructureKind::OuterMembrane ? kPeriplasmWidth : 0.0f);
        }
        if (!anyLayer && ctx.line(depth))
            capsuleOutline(halfL, r, 0.0f, depth, kMembraneLine);
    }

    // Nucleoide: región irregular más clara (excluye los ribosomas). Desaparece cuando el fago
    // degrada el ADN del huésped; en el bdeloplasto, el Bdellovibrio digiere el contenido.
    const StructureSpec* nuc = plan.find(StructureKind::Nucleoid);
    const float nl = nuc ? std::min(nuc->length, 1.2f * halfL) : 0.0f, nw = nuc ? std::min(nuc->width, 1.2f * r) : 0.0f;
    float dna = b.phageFraction < 0.0f ? 1.0f : std::clamp(1.0f - b.phageFraction / kHostDnaDegraded, 0.0f, 1.0f);
    if (bdelloplast)
        dna = 0.0f;
    if (nuc && dna > 0.0f && ctx.allowed(*nuc) && ctx.particle(nuc->width)) {
        Color col = kNucleoid;
        col.a = static_cast<unsigned char>(static_cast<float>(col.a) * dna);
        for (int k = 0; k < 5; ++k) { // lóbulos: el nucleoide no es una elipse regular
            const float t = (static_cast<float>(k) / 4.0f - 0.5f) * 0.6f * nl;
            const float dy = (hash01(b.id, 40u + static_cast<std::uint32_t>(k)) - 0.5f) * 0.25f * nw;
            const float ry = 0.5f * nw * (0.7f + 0.3f * hash01(b.id, 50u + static_cast<std::uint32_t>(k)));
            rlPushMatrix();
            rlTranslatef(t, dy, 0.0f);
            rlScalef(0.3f * nl, ry, 1.0f);
            DrawCircleV({0.0f, 0.0f}, 1.0f, col);
            rlPopMatrix();
        }
    }

    // Ribosomas: solo los de una rebanada del espesor de un corte fino (si no, en proyección se
    // tapan todos), fuera del nucleoide y dentro de la envoltura.
    const StructureSpec* rib = plan.find(StructureKind::Ribosomes);
    if (rib && rib->count > 0 && !bdelloplast && ctx.allowed(*rib) && ctx.particle(rib->width)) {
        const int n = std::min(ctx.particleBudget,
                               static_cast<int>(static_cast<float>(rib->count) * ctx.lod->sectionThickness / (2.0f * r)));
        const float inner = r - std::max(depth, 0.03f);
        int drawn = 0;
        for (int k = 0; k < n; ++k) {
            const Vec2 p = {(hash01(b.id, 10000u + static_cast<std::uint32_t>(k)) - 0.5f) * 2.0f * halfL,
                            (hash01(b.id, 90000u + static_cast<std::uint32_t>(k)) - 0.5f) * 2.0f * r};
            const float qx = std::max(0.0f, std::abs(p.x) - (halfL - r));
            if (qx * qx + p.y * p.y > inner * inner)
                continue;
            if (dna > 0.0f && nuc && (p.x * p.x) / (0.25f * nl * nl) + (p.y * p.y) / (0.25f * nw * nw) < 1.0f)
                continue;
            if (!inView(ctx, b.at, c, s, p))
                continue;
            DrawPoly(ray(p), 6, 0.5f * rib->width, 0.0f, kRibosome);
            ++drawn;
        }
        ctx.particleBudget -= drawn;
    }

    // Infección por fago: el que infectó queda pegado afuera con la cola contraída y la cabeza
    // vacía; pasado el eclipse se ensamblan las crías adentro, hasta el estallido.
    if (b.phageFraction >= 0.0f && ctx.model()) {
        const BodyPlan& phage = virusPlan(kSimulatedPhage);
        Vec2 p, n;
        capsulePoint(halfL, r, hash01(b.id, 9u), p, n);
        rlPushMatrix();
        rlTranslatef(p.x, p.y, 0.0f);
        rlRotatef(std::atan2(n.y, n.x) * RAD2DEG, 0.0f, 0.0f, 1.0f);
        drawPhage(phage, true, ctx);
        rlPopMatrix();
        const float mature = std::clamp((b.phageFraction - kPhageEclipse) / (1.0f - kPhageEclipse), 0.0f, 1.0f);
        const int progeny = static_cast<int>(mature * static_cast<float>(b.phageBurst));
        for (int k = 0; k < progeny; ++k) { // se muestran todas (son pocas), no una rebanada
            const Vec2 q = {(hash01(b.id, 500u + static_cast<std::uint32_t>(k)) - 0.5f) * 1.6f * (halfL - 0.15f),
                            (hash01(b.id, 700u + static_cast<std::uint32_t>(k)) - 0.5f) * 1.2f * (r - 0.12f)};
            rlPushMatrix();
            rlTranslatef(q.x, q.y, 0.0f);
            rlRotatef(360.0f * hash01(b.id, 900u + static_cast<std::uint32_t>(k)), 0.0f, 0.0f, 1.0f);
            drawPhage(phage, false, ctx);
            rlPopMatrix();
        }
    }
    if (bdelloplast && ctx.model() && ctx.particle(0.25f))
        drawBdelloInside(b, r);
    rlPopMatrix();
}

void drawBdellovibrioStructures(const BacteriumState& b, const BodyPlan& plan, StructureContext& ctx) {
    drawBacteriumStructures(b, plan, ctx);
}

void drawPhage(const BodyPlan& plan, bool contracted, StructureContext& ctx) {
    const StructureSpec* capsid = plan.find(StructureKind::Capsid);
    if (!capsid || !ctx.allowed(*capsid) || !ctx.particle(capsid->width))
        return;
    const StructureSpec* tail = plan.find(StructureKind::Tail);
    const StructureSpec* base = plan.find(StructureKind::Baseplate);
    const StructureSpec* fibers = plan.find(StructureKind::TailFibers);
    const StructureSpec* genome = plan.find(StructureKind::ViralGenome);
    const float bw = base ? base->width : 0.0f, bl = base ? base->length : 0.0f;
    // La vaina contraída queda en ~40% del largo y algo más ancha; el tubo interno atraviesa la
    // envoltura del huésped (Leiman et al. 2004).
    const float tl = tail ? (contracted ? 0.4f : 1.0f) * tail->length : 0.0f;
    const float tw = tail ? (contracted ? 1.4f : 1.0f) * tail->width : 0.0f;
    if (fibers && ctx.line(fibers->width)) {
        const float seg = 0.5f * fibers->length;
        for (int side : {-1, 1})
            for (int k = 0; k < fibers->count / 2; ++k) {
                const float spread = 0.6f + 0.3f * static_cast<float>(k);
                const Vec2 a = {0.5f * bw, static_cast<float>(side) * 0.5f * bl};
                const Vec2 knee = a + Vec2{0.35f * seg, static_cast<float>(side) * seg * spread};
                const Vec2 foot = knee + Vec2{-0.6f * seg, static_cast<float>(side) * 0.5f * seg};
                DrawLineEx(ray(a), ray(knee), fibers->width, kPhageLine);
                DrawLineEx(ray(knee), ray(foot), fibers->width, kPhageLine);
            }
    }
    if (base && ctx.line(base->width))
        DrawRectangleV({0.0f, -0.5f * bl}, {bw, bl}, kPhageLine);
    if (tail && ctx.line(tail->width)) {
        if (contracted && ctx.line(0.009f)) // tubo de la cola dentro de la envoltura
            DrawRectangleV({-0.035f, -0.0045f}, {0.035f + bw, 0.009f}, kPhageLine);
        DrawRectangleV({bw, -0.5f * tw}, {tl, tw}, kPhageLine);
    }
    // Cabeza: icosaedro alargado (en proyección, un hexágono estirado), llena de ADN o vacía.
    const float x0 = bw + tl + 0.004f, hl = capsid->length, hw = capsid->width;
    const Vector2 v[6] = {{x0, 0.0f}, {x0 + 0.22f * hl, 0.5f * hw}, {x0 + 0.78f * hl, 0.5f * hw},
                          {x0 + hl, 0.0f}, {x0 + 0.78f * hl, -0.5f * hw}, {x0 + 0.22f * hl, -0.5f * hw}};
    const Vector2 mid = {x0 + 0.5f * hl, 0.0f};
    const Color fill = contracted ? kCapsidEmpty : kCapsidFull;
    for (int k = 0; k < 6; ++k) // (el pase de estructuras dibuja sin descarte de caras)
        DrawTriangle(mid, v[k], v[(k + 1) % 6], fill);
    const float edge = std::max(0.003f, 1.0f / ctx.zoom);
    for (int k = 0; k < 6; ++k)
        DrawLineEx(v[k], v[(k + 1) % 6], edge, kPhageLine);
    // ADN empaquetado: la hebra real (~56 µm) va enrollada en capas; se dibujan algunas vueltas.
    if (!contracted && genome && ctx.model() && ctx.line(genome->width))
        for (int k = 1; k <= 4; ++k)
            DrawRectangleRoundedLinesEx({mid.x - 0.1f * hl * static_cast<float>(k), mid.y - 0.09f * hw * static_cast<float>(k),
                                             0.2f * hl * static_cast<float>(k), 0.18f * hw * static_cast<float>(k)},
                                            1.0f, 16, genome->width, kDna);
}

void drawCytoplasmicRibosomes(const StructureSpec& spec, const Vec2* ring, int count, Vec2 centroid, Vec2 heading,
                              Vec2 nucleus, float nucleusRadius, std::uint32_t seed, StructureContext& ctx) {
    if (!ctx.allowed(spec) || !ctx.particle(spec.width) || spec.perCubicMicron <= 0.0f || ctx.particleBudget <= 0)
        return;
    PROFILE_SCOPE("drawCytoplasmicRibosomes");
    const Vec2 side = {-heading.y, heading.x};
    auto toLocal = [&](Vec2 w) { const Vec2 d = w - centroid; return Vec2{dot(d, heading), dot(d, side)}; };
    auto toWorld = [&](Vec2 l) { return centroid + heading * l.x + side * l.y; };
    // Lo que se recorre: la caja de la vista en el marco de la célula, recortada por la caja de la célula.
    Vec2 lo = {1e30f, 1e30f}, hi = {-1e30f, -1e30f};
    for (const Vec2 corner : {Vec2{ctx.view.x, ctx.view.y}, Vec2{ctx.view.x + ctx.view.width, ctx.view.y},
                              Vec2{ctx.view.x, ctx.view.y + ctx.view.height},
                              Vec2{ctx.view.x + ctx.view.width, ctx.view.y + ctx.view.height}}) {
        const Vec2 l = toLocal(corner);
        lo = {std::min(lo.x, l.x), std::min(lo.y, l.y)};
        hi = {std::max(hi.x, l.x), std::max(hi.y, l.y)};
    }
    Vec2 clo = {1e30f, 1e30f}, chi = {-1e30f, -1e30f};
    for (int i = 0; i < count; ++i) {
        const Vec2 l = toLocal(ring[i]);
        clo = {std::min(clo.x, l.x), std::min(clo.y, l.y)};
        chi = {std::max(chi.x, l.x), std::max(chi.y, l.y)};
    }
    lo = {std::max(lo.x, clo.x), std::max(lo.y, clo.y)};
    hi = {std::min(hi.x, chi.x), std::min(hi.y, chi.y)};
    if (lo.x >= hi.x || lo.y >= hi.y)
        return;
    // Baldosas de 1 µm ancladas a la célula: cada una tiene siempre los mismos ribosomas.
    constexpr float kTile = 1.0f;
    const int perTile = static_cast<int>(spec.perCubicMicron * ctx.lod->sectionThickness * kTile * kTile);
    const int tx0 = static_cast<int>(std::floor(lo.x / kTile)), tx1 = static_cast<int>(std::floor(hi.x / kTile));
    const int ty0 = static_cast<int>(std::floor(lo.y / kTile)), ty1 = static_cast<int>(std::floor(hi.y / kTile));
    const float nr2 = nucleusRadius * nucleusRadius;
    for (int ty = ty0; ty <= ty1 && ctx.particleBudget > 0; ++ty)
        for (int tx = tx0; tx <= tx1 && ctx.particleBudget > 0; ++tx) {
            const auto tile = static_cast<std::uint32_t>((tx + 4096) * 8192 + (ty + 4096));
            // Baldosa entera adentro de la célula (sus 4 esquinas): no hace falta probar cada punto.
            bool whole = true;
            for (const Vec2 c : {Vec2{0, 0}, Vec2{kTile, 0}, Vec2{0, kTile}, Vec2{kTile, kTile}})
                whole = whole && polygon::contains(ring, count, toWorld(Vec2{tx * kTile, ty * kTile} + c));
            for (int k = 0; k < perTile && ctx.particleBudget > 0; ++k) {
                const Vec2 l = {(static_cast<float>(tx) + hash01(seed ^ tile, 2u * static_cast<std::uint32_t>(k))) * kTile,
                                (static_cast<float>(ty) + hash01(seed ^ tile, 2u * static_cast<std::uint32_t>(k) + 1u)) * kTile};
                const Vec2 w = toWorld(l);
                if (w.x < ctx.view.x || w.x > ctx.view.x + ctx.view.width || w.y < ctx.view.y || w.y > ctx.view.y + ctx.view.height)
                    continue;
                const Vec2 dn = w - nucleus;
                if (dot(dn, dn) < nr2 || (!whole && !polygon::contains(ring, count, w)))
                    continue;
                DrawPoly(ray(w), 6, 0.5f * spec.width, 0.0f, kRibosome);
                --ctx.particleBudget;
            }
        }
}

void drawEndomembranes(const BodyPlan& plan, Vec2 nucleus, float nucleusRadius, Vec2 heading, std::uint32_t seed,
                       StructureContext& ctx) {
    if (!ctx.model())
        return;
    // Orientado respecto del eje de la célula: gira con ella.
    const float a0 = std::atan2(heading.y, heading.x) + kTau * hash01(seed, 1u);
    if (const StructureSpec* er = plan.find(StructureKind::EndoplasmicReticulum); er && ctx.line(er->width)) {
        // RE rugoso: cisternas continuas con la envoltura nuclear, tachonadas de ribosomas 80S (~25 nm).
        for (int i = 0; i < 3; ++i) {
            const float rr = nucleusRadius + 0.35f * static_cast<float>(i + 1);
            const float span = er->length / rr;
            constexpr int kSegments = 32;
            Vec2 prev = nucleus + Vec2{std::cos(a0), std::sin(a0)} * rr;
            for (int k = 1; k <= kSegments; ++k) {
                const float a = a0 + span * static_cast<float>(k) / kSegments;
                const Vec2 p = nucleus + Vec2{std::cos(a), std::sin(a)} * rr;
                DrawLineEx(ray(prev), ray(p), er->width, kEr);
                prev = p;
            }
            if (ctx.particle(0.025f)) {
                const int dots = std::min(ctx.particleBudget, static_cast<int>(er->length / 0.08f));
                for (int k = 0; k < dots; ++k) {
                    const float a = a0 + span * (static_cast<float>(k) + 0.5f) / static_cast<float>(dots);
                    DrawPoly(ray(nucleus + Vec2{std::cos(a), std::sin(a)} * (rr + er->width)), 6, 0.0125f, 0.0f, kRibosome);
                }
                ctx.particleBudget -= dots;
            }
        }
    }
    if (const StructureSpec* golgi = plan.find(StructureKind::Golgi); golgi && ctx.line(0.03f)) {
        // Un dictiosoma: pila de 5 cisternas curvas (~30 nm, separadas ~70 nm) del otro lado del
        // núcleo, con vesículas en los bordes.
        const Vec2 dir = {std::cos(a0 + kPi), std::sin(a0 + kPi)};
        const Vec2 side = {-dir.y, dir.x};
        const Vec2 center = nucleus + dir * (nucleusRadius + 1.6f);
        for (int i = 0; i < 5; ++i) {
            const float half = 0.5f * golgi->length * (0.65f + 0.08f * static_cast<float>(i));
            const Vec2 mid = center + dir * (0.07f * static_cast<float>(i));
            constexpr int kSegments = 16;
            Vec2 prev{};
            for (int k = 0; k <= kSegments; ++k) {
                const float t = 2.0f * static_cast<float>(k) / kSegments - 1.0f;
                const Vec2 p = mid + side * (t * half) - dir * (0.25f * half * t * t); // curvas, cóncavas hacia el núcleo
                if (k > 0)
                    DrawLineEx(ray(prev), ray(p), 0.03f, kGolgi);
                prev = p;
            }
            if (ctx.particle(0.06f))
                for (float e : {-1.0f, 1.0f})
                    DrawCircleV(ray(mid + side * (e * (half + 0.06f)) - dir * (0.25f * half)), 0.03f, kGolgi);
        }
    }
}
