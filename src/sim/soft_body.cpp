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

void distanceStep(BodyParticles& bp, std::uint32_t a, std::uint32_t b, float rest, float f,
                  bool tensionOnly = false) {
    const Vec2 d = pos(bp, b) - pos(bp, a);
    const float len = length(d);
    if (tensionOnly && len <= rest)
        return;
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

    const float fe = relax(tuning.edgeRate * body.edgeScale, h);
    for (std::size_t i = 0; i < n; ++i)
        distanceStep(bp, body.ring[i], body.ring[(i + 1) % n], body.restEdge[i], fe,
                     body.edgeTensionOnly);

    const float fb = relax(tuning.bendRate * body.bendScale, h);
    for (std::size_t i = 0; i < n; ++i)
        localShapeStep(body, bp, i, fb);

    if (body.corticalTension > 0.0f) {
        const float fc = relax(body.corticalTension, h);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t s = body.ring[i];
            const Vec2 mid = (pos(bp, body.ring[(i + n - 1) % n]) + pos(bp, body.ring[(i + 1) % n])) * 0.5f;
            move(bp, s, (mid - pos(bp, s)) * fc);
        }
    }

    areaStep(body, bp, relax(tuning.areaRate * body.areaScale, h));

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

float perimeter(const RingBody& body, const BodyParticles& bp) {
    float p = 0.0f;
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        p += length(pos(bp, body.ring[(i + 1) % n]) - pos(bp, body.ring[i]));
    return p;
}

void addOrganelle(RingBody& body, std::uint32_t slot, Vec2 restLocal, float anchorRate) {
    const auto at = static_cast<std::ptrdiff_t>(body.organelleCount);
    body.interior.insert(body.interior.begin() + at, slot);
    body.restInterior.insert(body.restInterior.begin() + at, restLocal);
    body.anchorRate.insert(body.anchorRate.begin() + at, anchorRate);
    ++body.organelleCount;
}

void removeInterior(RingBody& body, std::size_t index) {
    const auto at = static_cast<std::ptrdiff_t>(index);
    body.interior.erase(body.interior.begin() + at);
    body.restInterior.erase(body.restInterior.begin() + at);
    body.anchorRate.erase(body.anchorRate.begin() + at);
    if (index < body.organelleCount)
        --body.organelleCount;
}

Vec2 toLocal(const RingBody& body, Vec2 worldPos) {
    return rotate(worldPos - body.centroid, std::cos(body.angle), -std::sin(body.angle));
}

void setRestArea(RingBody& body, float newSignedArea) {
    if (body.restArea == 0.0f || newSignedArea * body.restArea <= 0.0f)
        return;
    const float s = std::sqrt(newSignedArea / body.restArea);
    for (Vec2& q : body.restRing)
        q = q * s;
    for (float& e : body.restEdge)
        e *= s;
    for (Vec2& q : body.restInterior)
        q = q * s;
    body.restArea = newSignedArea;
}

namespace {
void setCircularRest(RingBody& body, float perim) {
    const std::size_t n = body.ring.size();
    const float radius = perim / (2.0f * 3.14159265f);
    const float dir = body.restArea >= 0.0f ? 1.0f : -1.0f; // misma orientación que el anillo
    body.restRing.resize(n);
    body.restEdge.assign(n, perim / static_cast<float>(n));
    for (std::size_t i = 0; i < n; ++i) {
        const float t = dir * 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(n);
        body.restRing[i] = {radius * std::cos(t), radius * std::sin(t)};
    }
}
} // namespace

void makePlastic(RingBody& body, BodyParticles& bp, float reservoirRate) {
    body.membraneReservoirRate = reservoirRate;
    body.shapeRate = 0.0f;
    // Córtex de ameba: membrana fluida que se estira y se dobla con facilidad.
    // Membrana de ameba: tensión casi nula (grandes reservorios de membrana; el remallado
    // mantiene el espaciado) y citoplasma que fluye: el volumen se conserva en ~1 s, no al
    // instante, para que los pseudópodos y los labios de la copa puedan llenarse.
    body.edgeScale = 0.0f;
    body.bendScale = 0.05f;
    body.areaScale = 0.001f;
    body.corticalTension = 5.0f;
    body.edgeTensionOnly = true;
    // La membrana activa cede en los contactos (las células pasivas descansan sobre el vidrio).
    for (std::uint32_t s : body.ring)
        bp.invMass[s] = 4.0f;
    body.restArea = area(body, bp);
    setCircularRest(body, perimeter(body, bp));
    // El interior se re-expresa en el marco nuevo (el ángulo cambia con la forma de reposo).
    bestFit(body, bp, body.centroid, body.angle);
    for (std::size_t k = 0; k < body.interior.size(); ++k)
        body.restInterior[k] = toLocal(body, pos(bp, body.interior[k]));
}

void adaptPlastic(RingBody& body, const BodyParticles& bp, float dt) {
    if (body.membraneReservoirRate <= 0.0f || body.restEdge.empty())
        return;
    const float rest = body.restEdge[0] * static_cast<float>(body.restEdge.size());
    const float target = perimeter(body, bp);
    const float s = 1.0f + (target / rest - 1.0f) * relax(body.membraneReservoirRate, dt);
    for (Vec2& q : body.restRing)
        q = q * s;
    for (float& e : body.restEdge)
        e *= s;
}

namespace {
void rebuildRest(RingBody& body, const BodyParticles& bp, bool keepArea);
} // namespace

void replaceRing(RingBody& body, const BodyParticles& bp, std::vector<std::uint32_t> ring) {
    body.ring = std::move(ring);
    rebuildRest(body, bp, false);
}

void resampleRing(RingBody& body, BodyParticles& bp, std::size_t count) {
    const std::size_t n = body.ring.size();
    std::vector<Vec2> pts(n);
    std::vector<float> cum(n + 1, 0.0f);
    for (std::size_t i = 0; i < n; ++i)
        pts[i] = pos(bp, body.ring[i]);
    for (std::size_t i = 0; i < n; ++i)
        cum[i + 1] = cum[i] + length(pts[(i + 1) % n] - pts[i]);
    const float total = cum[n];
    std::vector<std::uint32_t> ring;
    ring.reserve(count);
    std::size_t seg = 0;
    for (std::size_t k = 0; k < count; ++k) {
        const float target = total * static_cast<float>(k) / static_cast<float>(count);
        while (seg + 1 < n && cum[seg + 1] < target)
            ++seg;
        const float len = cum[seg + 1] - cum[seg];
        const float t = len > 0.0f ? (target - cum[seg]) / len : 0.0f;
        const Vec2 p = pts[seg] + (pts[(seg + 1) % n] - pts[seg]) * t;
        std::uint32_t s;
        if (k < n) {
            s = body.ring[k];
        } else {
            s = bp.allocate(BodyKind::Membrane, p.x, p.y, bp.radius[body.ring[0]],
                            bp.invMass[body.ring[0]], body.id);
        }
        bp.x[s] = p.x;
        bp.y[s] = p.y;
        ring.push_back(s);
    }
    for (std::size_t k = count; k < n; ++k)
        bp.release(body.ring[k]);
    body.ring = std::move(ring);
}

void reshapeRest(RingBody& body, const BodyParticles& bp, float rx, float ry) {
    const std::size_t n = body.ring.size();
    Vec2 c;
    for (std::uint32_t s : body.ring)
        c = c + pos(bp, s);
    c = c * (1.0f / static_cast<float>(n));
    // Eje principal por momentos de segundo orden.
    float sxx = 0.0f, syy = 0.0f, sxy = 0.0f;
    for (std::uint32_t s : body.ring) {
        const Vec2 d = pos(bp, s) - c;
        sxx += d.x * d.x;
        syy += d.y * d.y;
        sxy += d.x * d.y;
    }
    const float theta = 0.5f * std::atan2(2.0f * sxy, sxx - syy);
    const float co = std::cos(theta), si = std::sin(theta);
    body.restRing.resize(n);
    body.restEdge.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 d = rotate(pos(bp, body.ring[i]) - c, co, -si);
        const float phi = std::atan2(d.y, d.x);
        const float cp = std::cos(phi), sp = std::sin(phi);
        const float r = 1.0f / std::sqrt(cp * cp / (rx * rx) + sp * sp / (ry * ry));
        body.restRing[i] = {r * cp, r * sp};
    }
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    body.restArea = polygon::signedArea(body.restRing.data(), static_cast<int>(n));
    body.centroid = c;
    body.angle = theta;
    for (std::size_t k = 0; k < body.interior.size(); ++k)
        body.restInterior[k] = toLocal(body, pos(bp, body.interior[k]));
    refresh(body, bp);
}

void resizeRing(RingBody& body, const BodyParticles& bp, std::vector<std::uint32_t> ring) {
    body.ring = std::move(ring);
    rebuildRest(body, bp, true);
}

bool remesh(RingBody& body, BodyParticles& bp, float spacing) {
    if (body.membraneReservoirRate <= 0.0f)
        return false;
    const std::size_t n = body.ring.size();
    std::vector<std::uint32_t> ring;
    ring.reserve(n + 8);
    bool changed = false;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t a = body.ring[i], b = body.ring[(i + 1) % n];
        ring.push_back(a);
        const Vec2 pa = pos(bp, a), pb = pos(bp, b);
        // Tope de seguridad: una membrana no crece sin límite aunque algo la estire.
        if (length(pb - pa) > 1.6f * spacing && n + ring.size() - i < 4096) {
            const Vec2 mid = (pa + pb) * 0.5f;
            ring.push_back(bp.allocate(BodyKind::Membrane, mid.x, mid.y, bp.radius[a], bp.invMass[a], body.id));
            changed = true;
        }
    }
    for (std::size_t k = 0; ring.size() > 16 && k < ring.size();) {
        const std::uint32_t a = ring[k], b = ring[(k + 1) % ring.size()];
        if (length(pos(bp, b) - pos(bp, a)) < 0.5f * spacing) {
            bp.release(b);
            ring.erase(ring.begin() + static_cast<std::ptrdiff_t>((k + 1) % ring.size()));
            changed = true;
            continue;
        }
        ++k;
    }
    if (changed) {
        body.ring = std::move(ring);
        rebuildRest(body, bp, true);
    }
    return changed;
}

namespace {
void rebuildRest(RingBody& body, const BodyParticles& bp, bool keepArea) {
    const std::size_t n = body.ring.size();
    if (!keepArea)
        body.restArea = area(body, bp);
    if (body.membraneReservoirRate > 0.0f) {
        setCircularRest(body, perimeter(body, bp));
    } else {
        // Sin forma circular: la geometría actual (alineada al ángulo actual) es la de reposo.
        body.restRing.resize(n);
        body.restEdge.resize(n);
        Vec2 c;
        for (std::uint32_t s : body.ring)
            c = c + pos(bp, s);
        body.centroid = c * (1.0f / static_cast<float>(n));
        for (std::size_t i = 0; i < n; ++i)
            body.restRing[i] = toLocal(body, pos(bp, body.ring[i]));
        for (std::size_t i = 0; i < n; ++i)
            body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    }
    bestFit(body, bp, body.centroid, body.angle);
    for (std::size_t k = 0; k < body.interior.size(); ++k)
        body.restInterior[k] = toLocal(body, pos(bp, body.interior[k]));
    refresh(body, bp);
}
} // namespace

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
