#include "bio/division.hpp"

#include "bio/locomotion.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

Vec2 pos(const BodyParticles& bp, std::uint32_t s) { return {bp.x[s], bp.y[s]}; }

struct Axis {
    Vec2 c, h, side;
    float halfLength;
};

Axis axisOf(const RingBody& body, const BodyParticles& bp, float bodyHalfLength = 0.0f) {
    Axis a{body.centroid, bodyHeading(body), {}, 0.0f};
    a.side = {-a.h.y, a.h.x};
    for (std::uint32_t s : body.ring)
        a.halfLength = std::max(a.halfLength, std::abs(dot(pos(bp, s) - a.c, a.h)));
    if (bodyHalfLength > 0.0f)
        a.halfLength = bodyHalfLength;
    return a;
}

// El surco abarca la franja central de la célula.
float bandOf(const Axis& a) { return 0.3f * a.halfLength; }

// Fondo del surco de cada lado: el punto de la franja más cercano al eje.
void furrowTips(const RingBody& body, const BodyParticles& bp, const Axis& a, std::size_t& tipA,
                std::size_t& tipB, float& depthA, float& depthB) {
    depthA = depthB = std::numeric_limits<float>::max();
    tipA = tipB = 0;
    const float band = bandOf(a);
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const Vec2 r = pos(bp, body.ring[i]) - a.c;
        if (std::abs(dot(r, a.h)) > band)
            continue;
        const float across = dot(r, a.side);
        if (across >= 0.0f && across < depthA) {
            depthA = across;
            tipA = i;
        } else if (across < 0.0f && -across < depthB) {
            depthB = -across;
            tipB = i;
        }
    }
}

} // namespace

void applyFurrow(const RingBody& body, BodyParticles& bp, float speed, float bodyHalfLength) {
    const Axis a = axisOf(body, bp, bodyHalfLength);
    const float band = bandOf(a);
    for (std::uint32_t s : body.ring) {
        const Vec2 r = pos(bp, s) - a.c;
        const float w = 1.0f - std::abs(dot(r, a.h)) / band;
        if (w <= 0.0f)
            continue;
        const float toAxis = dot(r, a.side) >= 0.0f ? -1.0f : 1.0f;
        bp.vx[s] += a.side.x * toAxis * speed * w;
        bp.vy[s] += a.side.y * toAxis * speed * w;
    }
}

float furrowHalfWidth(const RingBody& body, const BodyParticles& bp, float bodyHalfLength) {
    const Axis a = axisOf(body, bp, bodyHalfLength);
    std::size_t ta, tb;
    float da, db;
    furrowTips(body, bp, a, ta, tb, da, db);
    return std::max(std::min(da, 1e6f), std::min(db, 1e6f));
}

bool furrowClosed(const RingBody& body, const BodyParticles& bp, std::size_t& tipA, std::size_t& tipB,
                  float bodyHalfLength) {
    const Axis a = axisOf(body, bp, bodyHalfLength);
    float da, db;
    furrowTips(body, bp, a, tipA, tipB, da, db);
    if (da == std::numeric_limits<float>::max() || db == std::numeric_limits<float>::max())
        return false;
    // El contacto de la membrana consigo misma deja las dos caras del surco a 2 radios (de punto a
    // arista), ~2,6 entre vértices: con 2,5 radios las células casi redondas (Cosmarium) y las de
    // radio de membrana grande quedaban con el surco cerrado sin partirse nunca.
    const float touch = 3.0f * bp.radius[body.ring[0]];
    return length(pos(bp, body.ring[tipA]) - pos(bp, body.ring[tipB])) < touch;
}

void divisionRestShape(const std::vector<Vec2>& mother, float scale, float t, std::vector<Vec2>& out) {
    float rx = 0.0f, ry = 0.0f;
    for (const Vec2& q : mother) {
        rx = std::max(rx, std::abs(q.x));
        ry = std::max(ry, std::abs(q.y));
    }
    // Hijas con la mitad del área y la misma forma: semiejes / √2, tocándose en x = 0.
    const float dx = rx / std::sqrt(2.0f), dy = ry / std::sqrt(2.0f);
    out.resize(mother.size());
    // Cada mitad del contorno de la madre recorre el contorno completo de una hija: el polo
    // de la madre va al polo de la hija y la cintura (θ = ±π/2) va al punto de pellizco.
    for (std::size_t i = 0; i < mother.size(); ++i) {
        const Vec2 q = mother[i];
        const float theta = std::atan2(q.y / ry, q.x / rx);
        const bool front = q.x >= 0.0f;
        const float phi = front ? 2.0f * theta : 2.0f * theta - 3.14159265f;
        const Vec2 center = {front ? dx : -dx, 0.0f};
        const Vec2 daughter = center + Vec2{dx * std::cos(phi), dy * std::sin(phi)};
        out[i] = (q + (daughter - q) * t) * scale;
    }
}

bool divisionRestShapeOutline(const std::vector<Vec2>& mother, const std::vector<Vec2>& outline, float outlineArea,
                              float scale, float t, std::vector<Vec2>& out) {
    const std::size_t n = mother.size();
    if (n < 6 || outline.size() < 3 || outlineArea <= 0.0f)
        return false;
    // La mitad anterior tiene que ser un solo tramo: arranca donde el anillo pasa de x < 0 a x ≥ 0.
    std::size_t start = n, transitions = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const bool front = mother[i].x >= 0.0f, prevFront = mother[(i + n - 1) % n].x >= 0.0f;
        if (front != prevFront) {
            ++transitions;
            if (front)
                start = i;
        }
    }
    if (transitions != 2 || start == n)
        return false;
    std::size_t frontCount = 0;
    while (frontCount < n && mother[(start + frontCount) % n].x >= 0.0f)
        ++frontCount;
    const std::size_t backCount = n - frontCount;
    if (frontCount < 3 || backCount < 3)
        return false;
    // Cada hija: la silueta con la mitad del área de la madre.
    float area = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        area += cross(mother[i], mother[(i + 1) % n]);
    const float k = std::sqrt(0.5f * std::abs(0.5f * area) / outlineArea);
    const bool reverse = area < 0.0f; // el anillo y la silueta (antihoraria) en el mismo sentido
    // Polos posterior y anterior de la silueta: los que tocan el plano de división.
    std::size_t rear = 0, fore = 0;
    for (std::size_t j = 1; j < outline.size(); ++j) {
        if (outline[j].x < outline[rear].x)
            rear = j;
        if (outline[j].x > outline[fore].x)
            fore = j;
    }
    thread_local std::vector<Vec2> a, b;
    polygon::sampleClosed(outline, static_cast<int>(frontCount), k, polygon::arcFraction(outline, rear, reverse), reverse, a);
    polygon::sampleClosed(outline, static_cast<int>(backCount), k, polygon::arcFraction(outline, fore, reverse), reverse, b);
    const Vec2 shiftA = outline[rear] * -k, shiftB = outline[fore] * -k;
    out.resize(n);
    for (std::size_t j = 0; j < frontCount; ++j) {
        const std::size_t i = (start + j) % n;
        out[i] = (mother[i] + (a[j] + shiftA - mother[i]) * t) * scale;
    }
    for (std::size_t j = 0; j < backCount; ++j) {
        const std::size_t i = (start + frontCount + j) % n;
        out[i] = (mother[i] + (b[j] + shiftB - mother[i]) * t) * scale;
    }
    return true;
}
