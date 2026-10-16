#include "render/tissue_renderer.hpp"

#include "game/stain.hpp"
#include "game/world.hpp"
#include "render/shapes.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kTau = 6.2831853f;
constexpr Color kWallEdge = {112, 108, 88, 210};    // borde refringente de la pared de celulosa
constexpr Color kWallInner = {246, 244, 232, 160};  // la pared misma, clara y algo brillante
constexpr Color kStarchGrain = {240, 238, 228, 190}; // almidón sin teñir: vidrioso, con capas
constexpr Color kStarchRing = {170, 166, 150, 150};
constexpr Color kNucleusNat = {150, 150, 160, 140};
constexpr Color kChromNat = {96, 92, 110, 220};
constexpr Color kChloroplast = {70, 140, 55, 220};
constexpr Color kRbcNat = {226, 180, 156, 215};     // hemoglobina sin teñir: rosado pajizo
constexpr Color kPlateletNat = {170, 160, 170, 200};

float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;
}

Color mixStain(Color base, const StainSpec* stain, StainTarget target, float opacity = 0.9f) {
    if (!stain)
        return base;
    const StainResult dye = evaluateStain(*stain, target);
    const float t = std::clamp(dye.alpha, 0.0f, 1.0f);
    auto mix = [&](unsigned char a, float b) { return static_cast<unsigned char>(a + (std::clamp(b, 0.0f, 1.0f) * 255.0f - a) * t); };
    Color o = {mix(base.r, dye.color.r), mix(base.g, dye.color.g), mix(base.b, dye.color.b), base.a};
    o.a = std::max<unsigned char>(base.a, static_cast<unsigned char>(255.0f * opacity * t));
    return o;
}

void ellipse(Vector2 c, float rx, float ry, float angle, Color color, float px) {
    const int n = std::clamp(static_cast<int>((rx + ry) / std::max(px, 0.05f)), 10, 40);
    const float ca = std::cos(angle), sa = std::sin(angle);
    Vector2 prev{};
    for (int k = 0; k <= n; ++k) {
        const float t = kTau * static_cast<float>(k) / static_cast<float>(n);
        const Vector2 p = {c.x + rx * std::cos(t) * ca - ry * std::sin(t) * sa, c.y + rx * std::cos(t) * sa + ry * std::sin(t) * ca};
        if (k > 0)
            DrawTriangle(c, p, prev, color);
        prev = p;
    }
}

void ellipseRing(Vector2 c, float rx, float ry, float angle, float width, Color color) {
    const int n = 32;
    const float ca = std::cos(angle), sa = std::sin(angle);
    Vector2 prev{};
    for (int k = 0; k <= n; ++k) {
        const float t = kTau * static_cast<float>(k) / static_cast<float>(n);
        const Vector2 p = {c.x + rx * std::cos(t) * ca - ry * std::sin(t) * sa, c.y + rx * std::cos(t) * sa + ry * std::sin(t) * ca};
        if (k > 0)
            DrawLineEx(prev, p, width, color);
        prev = p;
    }
}

// Granos de almidón (amiloplastos de la papa): ovoides de ~5–100 µm con capas concéntricas
// alrededor de un hilo excéntrico (el punto donde empezó a crecer). Con Lugol, azul violáceo
// oscuro: el yodo entra en la hélice de la amilosa; las capas, que alternan más y menos densas,
// se siguen viendo como anillos más oscuros, y el borde refringente queda claro.
void drawStarch(const TissueCell& cell, Vector2 off, float px, const StainSpec* stain) {
    const int grains = 6 + static_cast<int>(10.0f * hash01(cell.seed));
    const float cellRadius = std::sqrt(std::max(1.0f, cell.area) / 3.14159265f);
    const float size = cellRadius * 0.22f; // granos de ~5–35 µm en una célula de ~130 µm
    const bool stained = stain && evaluateStain(*stain, StainTarget::Starch).alpha > 0.05f;
    Color fill = mixStain(kStarchGrain, stain, StainTarget::Starch, 1.0f);
    if (stained) // no saturado: el grano teñido deja ver sus capas
        fill = {static_cast<unsigned char>(std::min(255, fill.r + 38)), static_cast<unsigned char>(std::min(255, fill.g + 30)),
                static_cast<unsigned char>(std::min(255, fill.b + 70)), 235};
    const Color ring = stained ? Color{static_cast<unsigned char>(fill.r / 2), static_cast<unsigned char>(fill.g / 2),
                                       static_cast<unsigned char>(fill.b / 2), 200}
                               : kStarchRing;
    for (int g = 0; g < grains; ++g) {
        const std::uint32_t h = cell.seed * 31u + static_cast<std::uint32_t>(g) * 2654435761u;
        const float r = size * (0.35f + 0.9f * hash01(h));
        const float a = kTau * hash01(h + 1u), d = std::sqrt(hash01(h + 2u)) * (cellRadius - r) * 0.8f;
        const Vector2 c = {cell.center.x + off.x + d * std::cos(a), cell.center.y + off.y + d * std::sin(a)};
        const float ang = kTau * hash01(h + 3u);
        ellipse(c, r, 0.75f * r, ang, fill, px);
        // Capas: elipses que se achican hacia el hilo, corrido hacia un polo del grano.
        const Vector2 hilum = {c.x - 0.45f * r * std::cos(ang), c.y - 0.45f * r * std::sin(ang)};
        const int layers = r / std::max(px, 0.05f) > 6.0f ? 4 : 0; // de lejos no se resuelven
        for (int layer = 1; layer <= layers; ++layer) {
            const float k = 0.2f * static_cast<float>(layer);
            const Vector2 lc = {hilum.x + (c.x - hilum.x) * k, hilum.y + (c.y - hilum.y) * k};
            ellipseRing(lc, r * k, 0.75f * r * k, ang, std::max(0.3f, 0.6f * px), ring);
        }
        if (layers > 0)
            drawCircleFast(hilum, std::max(0.4f, 0.06f * r), ring, px);
        ellipseRing(c, r, 0.75f * r, ang, std::max(0.4f, 0.8f * px), stained ? Color{20, 16, 40, 230} : kStarchRing);
        if (stained) // borde refringente
            ellipseRing(c, 1.04f * r, 0.78f * r, ang, std::max(0.3f, 0.5f * px), Color{230, 226, 240, 120});
    }
}

// Núcleo de una célula del meristema según su fase (la orceína tiñe la cromatina de rojo vinoso).
void drawMitosis(const TissueCell& cell, Vector2 off, float px, const StainSpec* stain) {
    const float side = std::sqrt(std::max(1.0f, cell.area));
    const Vector2 c = {cell.center.x + off.x, cell.center.y + off.y};
    const float r = 0.28f * side;
    const Color chrom = mixStain(kChromNat, stain, StainTarget::Nucleus, 1.0f);
    const Color nucleus = mixStain(kNucleusNat, stain, StainTarget::Nucleus, 0.5f);
    const float axis = kTau * hash01(cell.seed + 5u) * 0.25f; // el huso, más o menos a lo largo de la fila
    const Vector2 h = {std::cos(axis), std::sin(axis)}, s = {-h.y, h.x};
    const float thick = std::max(0.35f, 0.08f * r);
    switch (cell.phase) {
    case 0: // interfase: núcleo redondo con cromatina fina y nucléolos
        drawCircleFast(c, r, nucleus, px);
        drawRingFast(c, r - std::max(0.3f, px), r, 24, chrom, px);
        drawCircleFast({c.x + 0.3f * r, c.y - 0.2f * r}, 0.22f * r, chrom, px);
        break;
    case 1: // profase: la cromatina se condensa en hebras dentro del núcleo
        drawCircleFast(c, r, Color{nucleus.r, nucleus.g, nucleus.b, static_cast<unsigned char>(nucleus.a / 2)}, px);
        for (int k = 0; k < 8; ++k) {
            const std::uint32_t q = cell.seed * 7u + static_cast<std::uint32_t>(k);
            Vector2 p = {c.x + r * 0.7f * (hash01(q) - 0.5f) * 2.0f, c.y + r * 0.7f * (hash01(q + 9u) - 0.5f) * 2.0f};
            float a = kTau * hash01(q + 3u);
            for (int seg = 0; seg < 4; ++seg) {
                const Vector2 nxt = {p.x + 0.25f * r * std::cos(a), p.y + 0.25f * r * std::sin(a)};
                DrawLineEx(p, nxt, thick, chrom);
                p = nxt;
                a += 0.9f * (hash01(q + 17u + static_cast<std::uint32_t>(seg)) - 0.5f);
            }
        }
        break;
    case 2: // metafase: los cromosomas alineados en la placa ecuatorial
        for (int k = -6; k <= 6; ++k) {
            const Vector2 m = {c.x + s.x * 0.13f * r * static_cast<float>(k), c.y + s.y * 0.13f * r * static_cast<float>(k)};
            DrawLineEx({m.x - h.x * 0.35f * r, m.y - h.y * 0.35f * r}, {m.x + h.x * 0.35f * r, m.y + h.y * 0.35f * r}, thick, chrom);
        }
        break;
    case 3: // anafase: las cromátidas hermanas van hacia los polos (en V)
        for (int g = -1; g <= 1; g += 2) {
            for (int k = -5; k <= 5; ++k) {
                const float lateral = 0.13f * r * static_cast<float>(k);
                const Vector2 tip = {c.x + h.x * 0.9f * r * static_cast<float>(g) + s.x * lateral,
                                     c.y + h.y * 0.9f * r * static_cast<float>(g) + s.y * lateral};
                const Vector2 arm = {tip.x - h.x * 0.4f * r * static_cast<float>(g) + s.x * 0.1f * r * (k < 0 ? -1.0f : 1.0f),
                                     tip.y - h.y * 0.4f * r * static_cast<float>(g) + s.y * 0.1f * r * (k < 0 ? -1.0f : 1.0f)};
                DrawLineEx(tip, arm, thick, chrom);
            }
        }
        break;
    case 4: { // telofase: dos núcleos que se rearman y la placa celular entre ellos
        for (int g = -1; g <= 1; g += 2) {
            const Vector2 n = {c.x + h.x * 1.05f * r * static_cast<float>(g), c.y + h.y * 1.05f * r * static_cast<float>(g)};
            drawCircleFast(n, 0.55f * r, chrom, px);
        }
        DrawLineEx({c.x - s.x * 1.4f * r, c.y - s.y * 1.4f * r}, {c.x + s.x * 1.4f * r, c.y + s.y * 1.4f * r},
                   std::max(0.4f, 0.6f * px), Color{150, 144, 120, 180});
        break;
    }
    default: break;
    }
}

// Estoma: dos células oclusivas arriñonadas (con cloroplastos) alrededor del ostíolo. Ocupan el
// lugar de su célula: las paredes onduladas de las vecinas las tocan.
void drawStoma(const TissueCell& cell, Vector2 off, float px) {
    const float side = std::sqrt(std::max(1.0f, cell.area));
    const Vector2 c = {cell.center.x + off.x, cell.center.y + off.y};
    const float a = kTau * hash01(cell.seed + 11u) * 0.5f;
    const Vector2 h = {std::cos(a), std::sin(a)}, s = {-h.y, h.x};
    const float len = 0.47f * side, w = 0.2f * side;
    // Las células oclusivas juntas forman una elipse; cada una, la mitad, con la pared interna
    // (la del ostíolo) más gruesa.
    ellipse(c, len, 2.0f * w, a, Color{214, 226, 196, 225}, px);
    ellipseRing(c, len, 2.0f * w, a, std::max(0.6f, 1.2f * px), kWallEdge);
    DrawLineEx({c.x - h.x * len, c.y - h.y * len}, {c.x + h.x * len, c.y + h.y * len}, std::max(0.5f, px), kWallEdge);
    for (int g = -1; g <= 1; g += 2)
        for (int k = 0; k < 6; ++k) { // cloroplastos de la célula oclusiva (las de la epidermis no tienen)
            const float t = -0.62f + 0.25f * static_cast<float>(k);
            const float bow = 1.0f - t * t; // siguen la curva del riñón
            drawCircleFast({c.x + h.x * t * len + s.x * (0.45f + 0.5f * bow) * w * static_cast<float>(g),
                            c.y + h.y * t * len + s.y * (0.45f + 0.5f * bow) * w * static_cast<float>(g)},
                           0.13f * w + 0.4f, kChloroplast, px);
        }
    // El ostíolo (poro) entre las dos, con el engrosamiento de las paredes que lo bordean.
    ellipse(c, 0.55f * len, 0.3f * w, a, Color{120, 116, 100, 210}, px);
    ellipseRing(c, 0.55f * len, 0.3f * w, a, std::max(0.6f, 1.4f * px), Color{90, 86, 70, 220});
}

} // namespace

void drawTissue(const World& world, float offX, float offY, Rectangle view, float minSize, float px, float /*time*/,
                const StainSpec* stain) {
    const Tissue& t = world.tissue();
    if (!t.present())
        return;
    const Vector2 off = {offX, offY};
    const float wall = std::max(t.wallThickness, 1.2f * minSize / 1.5f);
    for (const TissueCell& cell : t.cells) {
        const float reach = std::sqrt(std::max(1.0f, cell.area));
        if (cell.center.x + offX + reach < view.x || cell.center.x + offX - reach > view.x + view.width ||
            cell.center.y + offY + reach < view.y || cell.center.y + offY - reach > view.y + view.height)
            continue;
        const std::size_t n = cell.wall.size();
        // Pared de celulosa: una banda clara con bordes refringentes (dos paredes vecinas son una sola
        // lámina: se dibuja la de cada célula y se superponen).
        for (std::size_t i = 0; i < n; ++i) {
            const Vector2 a = {cell.wall[i].x + offX, cell.wall[i].y + offY};
            const Vector2 b = {cell.wall[(i + 1) % n].x + offX, cell.wall[(i + 1) % n].y + offY};
            // Banda oscura (los dos bordes refringentes de la lámina) con el centro claro.
            DrawLineEx(a, b, std::max(wall, 1.6f * px), kWallEdge);
            DrawLineEx(a, b, std::max(0.4f * wall, 0.6f * px), kWallInner);
        }
        // Lejos (cada célula de unos pocos píxeles) solo se distinguen las paredes.
        if (reach / std::max(px, 1e-6f) < 12.0f)
            continue;
        switch (t.content) {
        case TissueContent::Starch: drawStarch(cell, off, px, stain); break;
        case TissueContent::Mitosis: drawMitosis(cell, off, px, stain); break;
        case TissueContent::Stomata:
            if (cell.guard)
                drawStoma(cell, off, px);
            break;
        case TissueContent::Protoplast: break; // lo dibuja el render de células
        }
    }
}

void drawBlood(const World& world, float offX, float offY, Rectangle view, float minSize, float px, const StainSpec* stain) {
    const Erythrocytes& b = world.erythrocytes();
    if (b.size() == 0)
        return;
    const Color rbc = mixStain(kRbcNat, stain, StainTarget::Hemoglobin, 0.95f);
    const Color platelet = mixStain(kPlateletNat, stain, StainTarget::Nucleus, 0.9f);
    const bool far = erythrocyte::kRadius / std::max(px, 1e-6f) < 3.0f; // de lejos: solo discos
    for (std::size_t i = 0; i < b.size(); ++i) {
        const Vector2 c = {b.x[i] + offX, b.y[i] + offY};
        if (c.x < view.x - 10.0f || c.x > view.x + view.width + 10.0f || c.y < view.y - 10.0f || c.y > view.y + view.height + 10.0f)
            continue;
        if (b.kind[i] == BloodParticle::Platelet) {
            drawCircleFast(c, std::max(erythrocyte::kPlateletRadius, 0.5f * minSize), platelet, px);
            continue;
        }
        const float v = b.volume[i];
        const float r = erythrocyte::kRadius;
        if (b.lysed[i]) { // fantasma: la membrana vacía, apenas visible
            drawRingFast(c, r - std::max(0.25f, 0.6f * px), r, 24, Color{200, 190, 180, 90}, px);
            continue;
        }
        if (far) {
            drawCircleFast(c, std::max(r, 0.5f * minSize), rbc, px);
            continue;
        }
        if (v < 0.88f) {
            // Crenado (equinocito): arrugado, con espículas regulares en el borde.
            const float rr = r * std::sqrt(std::max(0.5f, v));
            constexpr int kSpikes = 12;
            Vector2 prev{};
            for (int k = 0; k <= 2 * kSpikes; ++k) {
                const float ang = b.angle[i] + kTau * static_cast<float>(k) / (2.0f * kSpikes);
                const float rad = (k % 2 == 0) ? rr * 1.12f : rr * 0.9f;
                const Vector2 p = {c.x + rad * std::cos(ang), c.y + rad * std::sin(ang)};
                if (k > 0)
                    DrawTriangle(c, p, prev, rbc);
                prev = p;
            }
            continue;
        }
        // Disco bicóncavo (palidez central) que, al hincharse, se acerca a una esfera más chica y
        // uniforme (esferocito), hasta romperse.
        const float swell = std::clamp((v - 1.0f) / (b.critical[i] - 1.0f), 0.0f, 1.0f);
        const float radius = r * (1.0f - 0.15f * swell);
        drawCircleFast(c, radius, rbc, px);
        drawRingFast(c, radius - std::max(0.3f, 0.7f * px), radius, 28, Color{150, 100, 90, 160}, px);
        if (swell < 0.9f)
            drawCircleFast(c, radius * 0.38f * (1.0f - swell), Color{252, 238, 228, static_cast<unsigned char>(150.0f * (1.0f - swell))}, px);
    }
}
