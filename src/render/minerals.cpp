#include "render/minerals.hpp"
#include "render/shapes.hpp"

#include "game/mineral.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float kTau = 6.2831853f;

// Ruido estable por partícula (la orientación es su semilla: no cambia al reordenar el SoA).
float hash01(float seed, int k) {
    const float v = std::sin(seed * 12.9898f + static_cast<float>(k) * 78.233f) * 43758.5453f;
    return v - std::floor(v);
}

// Abanico desde el centro; los puntos van con el ángulo creciente (horario en pantalla), así que
// cada triángulo se da vuelta para que raylib no lo descarte.
void fan(Vector2 c, const Vector2* p, int n, Color color) {
    for (int i = 0; i < n; ++i)
        DrawTriangle(c, p[(i + 1) % n], p[i], color);
}

void outline(const Vector2* p, int n, float width, Color color) {
    for (int i = 0; i < n; ++i)
        DrawLineEx(p[i], p[(i + 1) % n], width, color);
}

// Polígono irregular de `n` vértices alrededor de c (granos angulosos).
void angular(Vector2 c, float r, float angle, int n, float jitter, Vector2* out) {
    for (int i = 0; i < n; ++i) {
        const float t = angle + kTau * (static_cast<float>(i) + 0.35f * (hash01(angle, i) - 0.5f)) / static_cast<float>(n);
        const float rr = r * (1.0f - jitter + 2.0f * jitter * hash01(angle, i + 17));
        out[i] = {c.x + rr * std::cos(t), c.y + rr * std::sin(t)};
    }
}

Vector2 toward(Vector2 p, Vector2 c, float k) { return {c.x + (p.x - c.x) * k, c.y + (p.y - c.y) * k}; }

// Estilos de dibujo con nombre (los eligen los datos del mineral, game/mineral).
enum class Style : std::uint8_t { Floc, Angular, Flake, Opaque, Globule, Frustule };

// Estilo de cada mineral, resuelto una vez por nombre (se recalcula si un paquete suma minerales).
Style styleOf(MineralKind kind) {
    static Style cache[kMaxMinerals];
    static int cached = 0;
    const int n = mineralCount();
    if (n != cached) {
        for (int k = 0; k < n; ++k) {
            const std::string& s = mineralInfo(static_cast<MineralKind>(k)).style;
            cache[k] = s == "grano_anguloso"        ? Style::Angular
                       : s == "escama"              ? Style::Flake
                       : s == "grano_opaco"         ? Style::Opaque
                       : s == "globulo_refringente" ? Style::Globule
                       : s == "frustula"            ? Style::Frustule
                                                    : Style::Floc;
        }
        cached = n;
    }
    const auto i = static_cast<int>(kind);
    return i < n ? cache[i] : Style::Floc;
}

} // namespace

void drawMineral(MineralKind kind, Vector2 c, float radius, float angle, float minSize) {
    const float r = std::max(radius, 0.5f * minSize);
    const float px = minSize / 1.5f; // un píxel en µm (minSize = 1,5 px)
    switch (styleOf(kind)) {
    case Style::Floc:
        drawCircleFast(c, r, {108, 98, 80, 170}, px);
        break;
    case Style::Angular: {
        Vector2 p[7], in[7];
        angular(c, r, angle, 7, 0.22f, p);
        for (int i = 0; i < 7; ++i)
            in[i] = toward(p[i], c, 0.72f);
        fan(c, p, 7, {222, 220, 208, 150});
        outline(p, 7, std::max(0.18f * r, 0.6f * minSize), {66, 66, 56, 220});
        outline(in, 7, std::max(0.08f * r, 0.4f * minSize), {252, 252, 244, 190}); // línea de Becke
        break;
    }
    case Style::Flake: {
        // Lámina hexagonal muy fina: casi invisible salvo los bordes y un reflejo.
        Vector2 p[6];
        for (int i = 0; i < 6; ++i)
            p[i] = {c.x + r * std::cos(angle + kTau * i / 6.0f), c.y + r * 0.85f * std::sin(angle + kTau * i / 6.0f)};
        fan(c, p, 6, {236, 232, 210, 90});
        outline(p, 6, std::max(0.07f * r, 0.4f * minSize), {150, 140, 108, 190});
        DrawLineEx(toward(p[0], c, 0.6f), toward(p[2], c, 0.6f), std::max(0.06f * r, 0.3f * minSize), {255, 255, 248, 200});
        break;
    }
    case Style::Opaque: {
        Vector2 p[8];
        angular(c, r, angle, 8, 0.3f, p);
        fan(c, p, 8, {124, 72, 40, 225});
        outline(p, 8, std::max(0.12f * r, 0.4f * minSize), {70, 40, 24, 235});
        break;
    }
    case Style::Globule:
        // Glóbulo muy refringente: borde oscuro y centro brillante amarillento.
        drawCircleFast(c, r, {232, 220, 128, 210}, px);
        drawRingFast(c, 0.8f * r, r, 24, {96, 88, 40, 220}, px);
        drawCircleFast({c.x - 0.25f * r, c.y - 0.25f * r}, 0.35f * r, {255, 252, 222, 230}, px);
        break;
    case Style::Frustule: {
        // Valva naviculoide vacía: contorno de sílice, rafe en el eje y estrías transversales.
        constexpr int kN = 28;
        Vector2 p[kN];
        const float ca = std::cos(angle), sa = std::sin(angle);
        for (int i = 0; i < kN; ++i) {
            const float t = kTau * static_cast<float>(i) / kN;
            const float x = r * std::cos(t);
            const float y = 0.27f * r * std::copysign(std::pow(std::abs(std::sin(t)), 0.8f), std::sin(t));
            p[i] = {c.x + x * ca - y * sa, c.y + x * sa + y * ca};
        }
        fan(c, p, kN, {232, 228, 206, 90});
        outline(p, kN, std::max(0.04f * r, 0.4f * minSize), {104, 100, 78, 220});
        const Vector2 a = {c.x + 0.85f * r * ca, c.y + 0.85f * r * sa}, b = {c.x - 0.85f * r * ca, c.y - 0.85f * r * sa};
        DrawLineEx(a, b, std::max(0.02f * r, 0.25f * minSize), {120, 116, 92, 200});
        for (int k = -6; k <= 6; ++k) {
            const float x = 0.12f * r * static_cast<float>(k);
            const float half = 0.22f * r * std::sqrt(std::max(0.0f, 1.0f - (x / r) * (x / r)));
            const Vector2 m = {c.x + x * ca, c.y + x * sa};
            DrawLineEx({m.x - half * sa, m.y + half * ca}, {m.x - 0.04f * r * sa, m.y + 0.04f * r * ca},
                       std::max(0.015f * r, 0.2f * minSize), {150, 144, 118, 150});
            DrawLineEx({m.x + half * sa, m.y - half * ca}, {m.x + 0.04f * r * sa, m.y - 0.04f * r * ca},
                       std::max(0.015f * r, 0.2f * minSize), {150, 144, 118, 150});
        }
        break;
    }
    }
}

void drawRock(const Vector2* pts, int n, Vector2 c, MineralKind kind, std::uint32_t seed, float edge) {
    const bool iron = styleOf(kind) == Style::Opaque; // grano opaco (óxido de hierro) o transparente
    // Cuerpo: el cuarzo es casi transparente y se aclara hacia el centro (actúa como lente).
    fan(c, pts, n, iron ? Color{150, 104, 70, 225} : Color{214, 212, 198, 190});
    std::vector<Vector2> inner(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        inner[static_cast<std::size_t>(i)] = toward(pts[i], c, 0.8f);
    fan(c, inner.data(), n, iron ? Color{170, 120, 82, 160} : Color{238, 236, 226, 150});
    // Fracturas e inclusiones fluidas.
    const float s = static_cast<float>(seed % 100000u) * 0.001f;
    for (int k = 0; k < 3; ++k) {
        const auto i = static_cast<int>(hash01(s, k) * static_cast<float>(n)) % n;
        const auto j = (i + n / 3 + static_cast<int>(hash01(s, k + 9) * static_cast<float>(n / 3))) % n;
        DrawLineEx(toward(pts[i], c, 0.85f), toward(pts[j], c, 0.5f), 0.4f * edge, {140, 136, 120, 120});
    }
    for (int k = 0; k < 5; ++k) {
        const auto i = static_cast<int>(hash01(s, k + 30) * static_cast<float>(n)) % n;
        const Vector2 q = toward(pts[i], c, 0.2f + 0.5f * hash01(s, k + 40));
        DrawCircleV(q, 0.5f * edge, {110, 108, 96, 140});
    }
    // Relieve: borde oscuro grueso y línea de Becke brillante por dentro.
    std::vector<Vector2> becke(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        becke[static_cast<std::size_t>(i)] = toward(pts[i], c, 0.93f);
    outline(becke.data(), n, 0.6f * edge, iron ? Color{210, 170, 130, 150} : Color{252, 252, 246, 210});
    outline(pts, n, edge, iron ? Color{70, 42, 26, 240} : Color{58, 58, 50, 235});
}
