#include "ui/being_icons.hpp"

#include "bio/species.hpp"
#include "core/units.hpp"
#include "render/cell_renderer.hpp"
#include "render/minerals.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {

Color withAlpha(Color c, bool enabled) {
    c.a = static_cast<unsigned char>(enabled ? c.a : c.a / 3);
    return c;
}

// Silueta de la especie (la superelipse de su CellSpec), escalada para entrar en el ícono.
void drawCellGlyph(const CellSpec& spec, Vector2 c, float scale, bool enabled) {
    constexpr int kN = 36;
    Vector2 pts[kN];
    for (int i = 0; i < kN; ++i) {
        const float t = 2.0f * units::kPi * static_cast<float>(i) / kN;
        const Vec2 q = superellipsePoint(spec.radiusX * scale, spec.radiusY * scale, spec.squareness, t, spec.taper);
        pts[i] = {c.x + q.x, c.y + q.y};
    }
    const Color fill = withAlpha({spec.r, spec.g, spec.b, 230}, enabled);
    for (int i = 0; i < kN; ++i) // en sentido antihorario en pantalla (raylib descarta el otro)
        DrawTriangle(c, pts[(i + 1) % kN], pts[i], fill);
    const Color edge = withAlpha({60, 64, 52, 220}, enabled);
    for (int i = 0; i < kN; ++i)
        DrawLineEx(pts[i], pts[(i + 1) % kN], 1.2f, edge);
    const float nucleus = std::max(1.5f, spec.nucleusFraction * std::min(spec.radiusX, spec.radiusY) * scale * 0.6f);
    DrawCircleV(c, nucleus, withAlpha({110, 105, 90, 200}, enabled));
}

void drawBeingIcon(Being b, Rectangle r, bool enabled) {
    const Vector2 c = {r.x + 0.5f * r.width, r.y + 0.5f * r.height};
    const float half = 0.4f * r.width;
    switch (b.kind) {
    case BeingKind::Species: {
        const SpeciesInfo& info = speciesInfo(b.species);
        const CellSpec& spec = info.spec;
        if (info.filament) { // tres células unidas extremo con extremo
            const float s = half / (3.0f * spec.radiusX);
            for (int k = -1; k <= 1; ++k)
                drawCellGlyph(spec, {c.x + 2.0f * spec.radiusX * s * static_cast<float>(k), c.y}, s, enabled);
        } else {
            drawCellGlyph(spec, c, half / std::max(spec.radiusX, spec.radiusY), enabled);
        }
        break;
    }
    case BeingKind::Bacterium:
    case BeingKind::Violaceum: {
        const Color col = withAlpha(b.kind == BeingKind::Violaceum ? Color{110, 60, 150, 255} : Color{120, 110, 80, 255}, enabled);
        const float pos[5][3] = {{-10, -8, 20}, {6, -10, -35}, {-4, 6, 70}, {10, 8, 10}, {0, -1, -60}};
        for (const auto& p : pos)
            DrawRectanglePro({c.x + p[0], c.y + p[1], 9, 4}, {4.5f, 2}, p[2], col);
        break;
    }
    case BeingKind::Bdellovibrio: {
        const Color col = withAlpha({90, 90, 70, 255}, enabled);
        const float pos[4][2] = {{-9, -6}, {7, -8}, {-3, 8}, {10, 6}};
        for (const auto& p : pos) {
            DrawCircleV({c.x + p[0], c.y + p[1]}, 2.0f, col);
            DrawLineEx({c.x + p[0], c.y + p[1]}, {c.x + p[0] - 5, c.y + p[1] + 2}, 1.0f, col);
        }
        break;
    }
    case BeingKind::Bacillus: { // bacilos grandes en cadena corta
        const Color col = withAlpha({92, 84, 108, 255}, enabled);
        for (int k = 0; k < 3; ++k)
            DrawRectanglePro({c.x - 12.0f + 12.0f * static_cast<float>(k), c.y + 3.0f * static_cast<float>(k % 2) - 3.0f, 12.0f, 5.5f},
                             {6.0f, 2.75f}, 8.0f * static_cast<float>(k) - 8.0f, col);
        break;
    }
    case BeingKind::Oscillatoria: // tricoma continuo con paredes transversales
        for (int k = 0; k < 8; ++k) {
            const float x = c.x - 16 + 4.0f * static_cast<float>(k), y = c.y + 6.0f * std::sin(0.5f * static_cast<float>(k));
            const float x2 = x + 4.0f, y2 = c.y + 6.0f * std::sin(0.5f * static_cast<float>(k + 1));
            DrawLineEx({x, y}, {x2, y2}, 5.0f, withAlpha({58, 104, 100, 255}, enabled));
            DrawLineEx({x2, y2 - 2.5f}, {x2, y2 + 2.5f}, 0.8f, withAlpha({36, 70, 68, 255}, enabled));
        }
        break;
    case BeingKind::Cyanobacterium:
        for (int k = 0; k < 7; ++k) {
            const bool heterocyst = k == 3;
            DrawCircleV({c.x - 15 + 5.0f * static_cast<float>(k), c.y}, heterocyst ? 3.2f : 2.5f,
                        withAlpha(heterocyst ? Color{170, 180, 120, 255} : Color{60, 120, 90, 255}, enabled));
        }
        break;
    case BeingKind::Chytrid: {
        const Color col = withAlpha({130, 120, 95, 255}, enabled);
        const float pos[3][2] = {{-8, -4}, {6, -8}, {2, 8}};
        for (const auto& p : pos) {
            DrawCircleV({c.x + p[0], c.y + p[1]}, 3.0f, col);
            DrawLineEx({c.x + p[0] - 3, c.y + p[1]}, {c.x + p[0] - 10, c.y + p[1] + 3}, 1.0f, col);
        }
        break;
    }
    case BeingKind::Allomyces: { // hifa que se bifurca en dos y esporangio de resistencia pardo
        const Color col = withAlpha({150, 128, 84, 255}, enabled);
        DrawLineEx({c.x - 14, c.y + 12}, {c.x - 2, c.y}, 2.2f, col);
        DrawLineEx({c.x - 2, c.y}, {c.x + 12, c.y - 12}, 2.0f, col);
        DrawLineEx({c.x - 2, c.y}, {c.x + 12, c.y + 8}, 2.0f, col);
        DrawCircleV({c.x + 13, c.y - 12}, 4.0f, withAlpha({110, 78, 44, 255}, enabled));
        break;
    }
    case BeingKind::Lichen: { // hifa abrazando un alga verde
        DrawCircleV({c.x + 6, c.y}, 6.0f, withAlpha({96, 158, 80, 255}, enabled));
        DrawLineEx({c.x - 14, c.y + 10}, {c.x - 4, c.y + 2}, 1.8f, withAlpha({120, 132, 104, 255}, enabled));
        DrawLineEx({c.x - 4, c.y + 2}, {c.x + 2, c.y - 6}, 1.4f, withAlpha({120, 132, 104, 255}, enabled));
        DrawLineEx({c.x - 4, c.y + 2}, {c.x + 2, c.y + 8}, 1.4f, withAlpha({120, 132, 104, 255}, enabled));
        break;
    }
    case BeingKind::Physarum: { // red de tubos amarillos
        const Color col = withAlpha({226, 190, 52, 255}, enabled);
        const Vector2 n[5] = {{c.x - 14, c.y + 8}, {c.x - 2, c.y - 2}, {c.x + 12, c.y - 10}, {c.x + 10, c.y + 10}, {c.x - 8, c.y - 12}};
        DrawLineEx(n[0], n[1], 4.0f, col);
        DrawLineEx(n[1], n[2], 3.0f, col);
        DrawLineEx(n[1], n[3], 2.4f, col);
        DrawLineEx(n[1], n[4], 1.6f, col);
        DrawLineEx(n[2], n[3], 1.4f, col);
        break;
    }
    case BeingKind::OatFlake: {
        DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 12.0f, 8.0f, withAlpha({224, 212, 176, 255}, enabled));
        DrawEllipseLines(static_cast<int>(c.x), static_cast<int>(c.y), 12.0f, 8.0f, withAlpha({150, 136, 100, 255}, enabled));
        break;
    }
    case BeingKind::Saprolegnia: {
        const Color col = withAlpha({120, 115, 95, 255}, enabled);
        const Vector2 base = {c.x - 14, c.y + 10};
        const Vector2 mid = {c.x - 2, c.y};
        DrawLineEx(base, mid, 1.6f, col);
        DrawLineEx(mid, {c.x + 14, c.y - 12}, 1.6f, col);
        DrawLineEx(mid, {c.x + 6, c.y + 12}, 1.4f, col);
        DrawLineEx({c.x + 6, c.y - 5}, {c.x - 4, c.y - 14}, 1.2f, col);
        DrawEllipse(static_cast<int>(c.x + 14), static_cast<int>(c.y - 12), 2.5f, 4.0f, col); // zoosporangio
        break;
    }
    case BeingKind::Phage: {
        const Color col = withAlpha({80, 80, 90, 255}, enabled);
        DrawPolyLinesEx({c.x, c.y - 7}, 6, 7.0f, 0.0f, 1.6f, col); // cápside
        DrawLineEx({c.x, c.y}, {c.x, c.y + 10}, 2.0f, col);       // cola
        DrawLineEx({c.x, c.y + 10}, {c.x - 6, c.y + 15}, 1.0f, col);
        DrawLineEx({c.x, c.y + 10}, {c.x + 6, c.y + 15}, 1.0f, col);
        break;
    }
    case BeingKind::Clay: { // un flóculo de partículas finas
        const float pos[7][2] = {{-6, -3}, {-2, -6}, {3, -4}, {6, 1}, {1, 4}, {-4, 3}, {-1, -1}};
        for (const auto& p : pos)
            drawMineral(MineralKind::Clay, {c.x + p[0], c.y + p[1]}, 2.6f, 0.0f, 0.0f);
        break;
    }
    case BeingKind::Quartz:
    case BeingKind::Mica:
    case BeingKind::IronOxide:
    case BeingKind::Sulfur:
        drawMineral(beingMineral(b.kind), c, 0.7f * half, 0.6f, 0.0f);
        break;
    case BeingKind::Frustule: drawMineral(MineralKind::Frustule, c, half, -0.5f, 0.0f); break;
    case BeingKind::Rock: {
        Vector2 pts[32];
        for (int i = 0; i < 32; ++i) {
            const float t = 6.2831853f * static_cast<float>(i) / 32.0f;
            const float rr = half * (1.0f + 0.12f * std::cos(2.0f * t + 0.7f) + 0.05f * std::cos(3.0f * t + 2.0f));
            pts[i] = {c.x + rr * std::cos(t), c.y + rr * std::sin(t)};
        }
        drawRock(pts, 32, c, MineralKind::Quartz, 12345u, 2.2f);
        break;
    }
    case BeingKind::Sediment:
        drawMineral(MineralKind::Quartz, {c.x - 6, c.y - 5}, 7.0f, 0.3f, 0.0f);
        drawMineral(MineralKind::IronOxide, {c.x + 8, c.y + 2}, 5.0f, 1.1f, 0.0f);
        drawMineral(MineralKind::Mica, {c.x - 5, c.y + 9}, 5.0f, 0.2f, 0.0f);
        for (const float p : {-10.0f, 2.0f, 10.0f})
            drawMineral(MineralKind::Clay, {c.x + p, c.y - 11 + 0.5f * p}, 1.6f, 0.0f, 0.0f);
        break;
    case BeingKind::Count: break;
    }
}

constexpr int kCell = 56; // [px] de cada ícono en el atlas
RenderTexture2D gAtlas{};
int gColumns = 8;

} // namespace

void buildBeingIcons() {
    gColumns = 12;
    const int rows = (beingCount() + gColumns - 1) / gColumns;
    gAtlas = LoadRenderTexture(gColumns * kCell, rows * kCell);
    SetTextureFilter(gAtlas.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(gAtlas);
    ClearBackground({244, 240, 226, 255}); // fondo claro de campo claro: los íconos se ven como en el microscopio
    for (int i = 0; i < beingCount(); ++i) {
        const Rectangle r = {static_cast<float>((i % gColumns) * kCell), static_cast<float>((i / gColumns) * kCell),
                             static_cast<float>(kCell), static_cast<float>(kCell)};
        drawBeingIcon(beingAt(i), r, true);
    }
    EndTextureMode();
}

void freeBeingIcons() {
    if (gAtlas.id != 0)
        UnloadRenderTexture(gAtlas);
    gAtlas = {};
}

IconUv beingIcon(int index) {
    // Las texturas de render están invertidas en Y.
    const float w = static_cast<float>(gAtlas.texture.width), h = static_cast<float>(gAtlas.texture.height);
    const float x = static_cast<float>((index % gColumns) * kCell), y = static_cast<float>((index / gColumns) * kCell);
    IconUv uv;
    uv.texture = gAtlas.texture.id;
    uv.u0 = x / w;
    uv.u1 = (x + kCell) / w;
    uv.v0 = 1.0f - y / h;
    uv.v1 = 1.0f - (y + kCell) / h;
    return uv;
}

} // namespace ui
