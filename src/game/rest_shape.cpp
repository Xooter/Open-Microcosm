#include "game/rest_shape.hpp"

#include "sim/polygon.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// Perfil axial de un contorno: bordes superior e inferior por franjas de x y el volumen acumulado (de
// revolución) desde el polo posterior, normalizado.
constexpr int kBins = 32;
struct AxialProfile {
    float x0 = 0.0f, dx = 1.0f;
    std::array<float, kBins> lo{}, hi{};
    std::array<float, kBins + 1> cum{};
};

void axialProfile(const std::vector<Vec2>& ring, AxialProfile& p) {
    float x0 = 1e9f, x1 = -1e9f;
    for (const Vec2& q : ring) {
        x0 = std::min(x0, q.x);
        x1 = std::max(x1, q.x);
    }
    p.x0 = x0;
    p.dx = std::max(1e-3f, (x1 - x0) / kBins);
    const std::size_t n = ring.size();
    p.cum[0] = 0.0f;
    for (int b = 0; b < kBins; ++b) {
        const float x = x0 + (static_cast<float>(b) + 0.5f) * p.dx;
        float lo = 1e9f, hi = -1e9f;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2 a = ring[i], c = ring[(i + 1) % n];
            if ((a.x - x) * (c.x - x) > 0.0f || a.x == c.x)
                continue;
            const float y = a.y + (c.y - a.y) * (x - a.x) / (c.x - a.x);
            lo = std::min(lo, y);
            hi = std::max(hi, y);
        }
        if (hi < lo)
            lo = hi = 0.0f;
        p.lo[static_cast<std::size_t>(b)] = lo;
        p.hi[static_cast<std::size_t>(b)] = hi;
        const float r = 0.5f * (hi - lo);
        p.cum[static_cast<std::size_t>(b) + 1] = p.cum[static_cast<std::size_t>(b)] + r * r;
    }
    const float total = std::max(1e-6f, p.cum[kBins]);
    for (float& v : p.cum)
        v /= total;
}

int binOf(const AxialProfile& p, float x) { return std::clamp(static_cast<int>((x - p.x0) / p.dx), 0, kBins - 1); }

float fractionAt(const AxialProfile& p, float x) {
    const float b = std::clamp((x - p.x0) / p.dx, 0.0f, static_cast<float>(kBins));
    const int i = std::min(kBins - 1, static_cast<int>(b));
    return p.cum[static_cast<std::size_t>(i)] + (p.cum[static_cast<std::size_t>(i) + 1] - p.cum[static_cast<std::size_t>(i)]) * (b - static_cast<float>(i));
}

float xAtFraction(const AxialProfile& p, float f) {
    int i = 0;
    while (i + 1 < kBins && p.cum[static_cast<std::size_t>(i) + 1] < f)
        ++i;
    const float a = p.cum[static_cast<std::size_t>(i)], b = p.cum[static_cast<std::size_t>(i) + 1];
    const float t = b > a ? std::clamp((f - a) / (b - a), 0.0f, 1.0f) : 0.5f;
    return p.x0 + (static_cast<float>(i) + t) * p.dx;
}

} // namespace

namespace rest_shape {

void apply(RingBody& body, const std::vector<Vec2>& shape, float areaScale, float previousAreaScale) {
    const std::size_t n = body.restRing.size();
    if (shape.size() != n || n < 3)
        return;
    thread_local std::vector<Vec2> before;
    thread_local AxialProfile from, to;
    before = body.restRing;
    const float a = std::abs(polygon::signedArea(shape.data(), static_cast<int>(n)));
    const float area = std::abs(body.restArea) * areaScale / std::max(1e-6f, previousAreaScale);
    const float g = a > 0.0f ? std::sqrt(area / a) : 1.0f;
    Vec2 mean;
    for (const Vec2& q : shape)
        mean = mean + q;
    mean = mean * (1.0f / static_cast<float>(n));
    for (std::size_t i = 0; i < n; ++i)
        body.restRing[i] = (shape[i] - mean) * g;
    body.restArea = body.restArea >= 0.0f ? area : -area;
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    if (body.restInterior.empty())
        return;
    axialProfile(before, from);
    axialProfile(body.restRing, to);
    for (Vec2& q : body.restInterior) {
        const int b0 = binOf(from, q.x);
        const float lo0 = from.lo[static_cast<std::size_t>(b0)], hi0 = from.hi[static_cast<std::size_t>(b0)];
        // Lejos de los polos y del borde: con cada ida y vuelta de la forma (contracciones repetidas) los
        // organelos derivaban hacia los extremos y quedaban fuera del contorno (trababan el surco).
        const float x = xAtFraction(to, std::clamp(fractionAt(from, q.x), 0.06f, 0.94f));
        const int b1 = binOf(to, x);
        const float lo1 = to.lo[static_cast<std::size_t>(b1)], hi1 = to.hi[static_cast<std::size_t>(b1)];
        const float half0 = 0.5f * (hi0 - lo0), half1 = 0.5f * (hi1 - lo1);
        const float rel = half0 > 1e-3f ? std::clamp((q.y - 0.5f * (lo0 + hi0)) / half0, -0.75f, 0.75f) : 0.0f;
        q = {x, 0.5f * (lo1 + hi1) + rel * half1};
    }
}

} // namespace rest_shape
