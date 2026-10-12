#include "sim/soft_body.hpp"

#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace soft_body {

namespace {

float relax(float rate, float h) { return 1.0f - std::exp(-rate * h); }

Vec2 pos(const BodyParticles& bp, std::uint32_t s) { return {bp.x[s], bp.y[s]}; }

void move(BodyParticles& bp, std::uint32_t s, Vec2 d) {
    bp.x[s] += d.x;
    bp.y[s] += d.y;
}

void distanceStep(BodyParticles& bp, std::uint32_t a, std::uint32_t b, float rest, float f) {
    const Vec2 d = pos(bp, b) - pos(bp, a);
    const float len = length(d);
    const float w = bp.invMass[a] + bp.invMass[b];
    if (len < 1e-6f || w <= 0.0f)
        return;
    const Vec2 corr = d * (f * (len - rest) / (len * w));
    move(bp, a, corr * bp.invMass[a]);
    move(bp, b, corr * -bp.invMass[b]);
}

// Flexión: shape matching local sobre una ventana de 5 puntos centrada en i. La rotación
// óptima nunca refleja, así que un pliegue (punto del lado equivocado) se deshace solo.
// Una restricción de distancia i↔i+2 no lo distingue, y deja que la membrana se anude.
void localShapeStep(const RingBody& body, BodyParticles& bp, std::size_t i, float f) {
    constexpr int kHalf = 2;
    const std::size_t n = body.ring.size();
    Vec2 c, q;
    for (int k = -kHalf; k <= kHalf; ++k) {
        const std::size_t j = (i + n + k) % n;
        c = c + pos(bp, body.ring[j]);
        q = q + body.restRing[j];
    }
    const float inv = 1.0f / (2 * kHalf + 1);
    c = c * inv;
    q = q * inv;
    float sdot = 0.0f, scross = 0.0f;
    for (int k = -kHalf; k <= kHalf; ++k) {
        const std::size_t j = (i + n + k) % n;
        const Vec2 p = pos(bp, body.ring[j]) - c, r = body.restRing[j] - q;
        sdot += dot(r, p);
        scross += cross(r, p);
    }
    const float a = std::atan2(scross, sdot);
    const Vec2 goal = c + rotate(body.restRing[i] - q, std::cos(a), std::sin(a));
    move(bp, body.ring[i], (goal - pos(bp, body.ring[i])) * f);
}

void gatherPoly(RingBody& body, const BodyParticles& bp) {
    body.poly.resize(body.ring.size());
    for (std::size_t i = 0; i < body.ring.size(); ++i)
        body.poly[i] = pos(bp, body.ring[i]);
}

// Centroide y rotación óptima del anillo respecto a la forma de reposo (shape matching 2D).
void bestFit(const RingBody& body, const BodyParticles& bp, Vec2& c, float& angle) {
    const std::size_t n = body.ring.size();
    c = {};
    for (std::uint32_t s : body.ring)
        c = c + pos(bp, s);
    c = c * (1.0f / static_cast<float>(n));
    float sdot = 0.0f, scross = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 p = pos(bp, body.ring[i]) - c, q = body.restRing[i];
        sdot += dot(q, p);
        scross += cross(q, p);
    }
    angle = std::atan2(scross, sdot);
}

void areaStep(RingBody& body, BodyParticles& bp, float f) {
    const std::size_t n = body.ring.size();
    float a = 0.0f, denom = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        a += cross(pos(bp, body.ring[i]), pos(bp, body.ring[(i + 1) % n]));
    a *= 0.5f;
    // ∂A/∂x_i = ½ (y_{i+1} − y_{i−1}, x_{i−1} − x_{i+1})
    auto grad = [&](std::size_t i) {
        const Vec2 next = pos(bp, body.ring[(i + 1) % n]), prev = pos(bp, body.ring[(i + n - 1) % n]);
        return Vec2{0.5f * (next.y - prev.y), 0.5f * (prev.x - next.x)};
    };
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 g = grad(i);
        denom += bp.invMass[body.ring[i]] * dot(g, g);
    }
    if (denom <= 0.0f)
        return;
    const float s = -f * (a - body.restArea) / denom;
    // Se calculan todos los gradientes antes de mover para no sesgar el resultado.
    body.poly.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        body.poly[i] = grad(i);
    for (std::size_t i = 0; i < n; ++i)
        move(bp, body.ring[i], body.poly[i] * (s * bp.invMass[body.ring[i]]));
}

void confineOrganelle(RingBody& body, BodyParticles& bp, std::uint32_t s) {
    const int n = static_cast<int>(body.poly.size());
    const Vec2 p = pos(bp, s);
    const float r = bp.radius[s];
    const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), n, p);
    const bool inside = polygon::contains(body.poly.data(), n, p);
    if (inside && cl.dist2 >= r * r)
        return;
    const Vec2 target = cl.point + polygon::inwardNormal(cl.edgeDir, body.restArea) * r;
    const Vec2 delta = target - p;
    // Mitad la absorbe el organelo y mitad la membrana: un núcleo grande abulta la célula.
    move(bp, s, delta * 0.5f);
    const std::uint32_t a = body.ring[cl.edge], b = body.ring[(cl.edge + 1) % n];
    move(bp, a, delta * (-0.5f * (1.0f - cl.t)));
    move(bp, b, delta * (-0.5f * cl.t));
}

} // namespace

void predict(RingBody& body, BodyParticles& bp, float h) {
    for (std::uint32_t s : body.ring)
        move(bp, s, Vec2{bp.vx[s], bp.vy[s]} * h);
    for (std::uint32_t s : body.interior)
        move(bp, s, Vec2{bp.vx[s], bp.vy[s]} * h);
}

void solve(RingBody& body, BodyParticles& bp, const SoftBodyTuning& tuning, float h) {
    const std::size_t n = body.ring.size();

    const float fe = relax(tuning.edgeRate, h);
    for (std::size_t i = 0; i < n; ++i)
        distanceStep(bp, body.ring[i], body.ring[(i + 1) % n], body.restEdge[i], fe);

    const float fb = relax(tuning.bendRate, h);
    for (std::size_t i = 0; i < n; ++i)
        localShapeStep(body, bp, i, fb);

    areaStep(body, bp, relax(tuning.areaRate, h));

    // Memoria de forma: la membrana tiende a la forma de reposo rotada y trasladada.
    bestFit(body, bp, body.centroid, body.angle);
    const float c = std::cos(body.angle), sn = std::sin(body.angle);
    const float fs = relax(body.shapeRate * tuning.shapeScale, h);
    if (fs > 0.0f) {
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2 goal = body.centroid + rotate(body.restRing[i], c, sn);
            move(bp, body.ring[i], (goal - pos(bp, body.ring[i])) * fs);
        }
    }
    // Los organelos anclados al citoesqueleto siguen el marco de la célula.
    for (std::size_t k = 0; k < body.interior.size(); ++k) {
        if (body.anchorRate[k] <= 0.0f)
            continue;
        const std::uint32_t s = body.interior[k];
        const Vec2 goal = body.centroid + rotate(body.restInterior[k], c, sn);
        move(bp, s, (goal - pos(bp, s)) * relax(body.anchorRate[k], h));
    }

    gatherPoly(body, bp);
    for (std::uint32_t k = 0; k < body.organelleCount; ++k)
        confineOrganelle(body, bp, body.interior[k]);

    // Los organelos no se superponen entre sí.
    for (std::uint32_t i = 0; i < body.organelleCount; ++i) {
        for (std::uint32_t j = i + 1; j < body.organelleCount; ++j) {
            const std::uint32_t a = body.interior[i], b = body.interior[j];
            const Vec2 d = pos(bp, b) - pos(bp, a);
            const float len = length(d), minD = bp.radius[a] + bp.radius[b];
            const float w = bp.invMass[a] + bp.invMass[b];
            if (len >= minD || len < 1e-6f || w <= 0.0f)
                continue;
            const Vec2 corr = d * ((minD - len) / (len * w));
            move(bp, a, corr * -bp.invMass[a]);
            move(bp, b, corr * bp.invMass[b]);
        }
    }
}

void confineGranules(RingBody& body, BodyParticles& bp) {
    gatherPoly(body, bp);
    const int n = static_cast<int>(body.poly.size());
    for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k) {
        const std::uint32_t s = body.interior[k];
        Vec2 p = pos(bp, s);
        const float r = bp.radius[s];
        // Fuera de los organelos (no pueden atravesar el núcleo ni la vacuola).
        for (std::uint32_t o = 0; o < body.organelleCount; ++o) {
            const std::uint32_t os = body.interior[o];
            const Vec2 d = p - pos(bp, os);
            const float len = length(d), minD = r + bp.radius[os];
            if (len < minD && len > 1e-6f)
                p = pos(bp, os) + d * (minD / len);
        }
        const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), n, p);
        if (!polygon::contains(body.poly.data(), n, p) || cl.dist2 < r * r)
            p = cl.point + polygon::inwardNormal(cl.edgeDir, body.restArea) * r;
        bp.x[s] = p.x;
        bp.y[s] = p.y;
    }
}

void refresh(RingBody& body, const BodyParticles& bp) {
    gatherPoly(body, bp);
    bestFit(body, bp, body.centroid, body.angle);
    Vec2 lo = body.poly.front(), hi = body.poly.front();
    float margin = 0.0f;
    for (std::size_t i = 0; i < body.poly.size(); ++i) {
        lo = {std::min(lo.x, body.poly[i].x), std::min(lo.y, body.poly[i].y)};
        hi = {std::max(hi.x, body.poly[i].x), std::max(hi.y, body.poly[i].y)};
        margin = std::max(margin, bp.radius[body.ring[i]]);
    }
    body.aabbMin = lo - Vec2{margin, margin};
    body.aabbMax = hi + Vec2{margin, margin};
}

void wrap(RingBody& body, BodyParticles& bp, float width, float height) {
    const Vec2 c = body.centroid;
    const Vec2 shift = {c.x < 0.0f ? width : (c.x >= width ? -width : 0.0f),
                        c.y < 0.0f ? height : (c.y >= height ? -height : 0.0f)};
    if (shift.x == 0.0f && shift.y == 0.0f)
        return;
    for (std::uint32_t s : body.ring)
        move(bp, s, shift);
    for (std::uint32_t s : body.interior)
        move(bp, s, shift);
    refresh(body, bp);
}

float area(const RingBody& body, const BodyParticles& bp) {
    float a = 0.0f;
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        a += cross(pos(bp, body.ring[i]), pos(bp, body.ring[(i + 1) % n]));
    return 0.5f * a;
}

float shapeError(const RingBody& body, const BodyParticles& bp) {
    Vec2 c;
    float angle;
    bestFit(body, bp, c, angle);
    const float co = std::cos(angle), sn = std::sin(angle);
    float sum = 0.0f;
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const Vec2 d = pos(bp, body.ring[i]) - (c + rotate(body.restRing[i], co, sn));
        sum += dot(d, d);
    }
    return std::sqrt(sum / static_cast<float>(body.ring.size()));
}

} // namespace soft_body
