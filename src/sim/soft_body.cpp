#include "sim/soft_body.hpp"

#include "core/finite.hpp"

#include "sim/polygon.hpp"

#include <algorithm>
#include <limits>
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
    // La rotación óptima sale directo de las sumas: cos = Σ·/|Σ|, sen = Σ×/|Σ| (sin atan2).
    const float len = std::sqrt(sdot * sdot + scross * scross);
    if (len < 1e-12f)
        return;
    const Vec2 goal = c + rotate(body.restRing[i] - q, sdot / len, scross / len);
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

bool recoverNonFinite(RingBody& body, BodyParticles& bp) {
    bool bad = false;
    for (std::uint32_t s : body.ring)
        bad |= !finiteNumber(bp.x[s]) || !finiteNumber(bp.y[s]);
    for (std::uint32_t s : body.interior)
        bad |= !finiteNumber(bp.x[s]) || !finiteNumber(bp.y[s]);
    if (!bad)
        return false;
    // El centro y el ángulo del último refresh pueden ser NaN también (si el cuerpo ya venía roto):
    // entonces se rearma alrededor de las partículas que sigan finitas, o del último centro finito.
    Vec2 center = body.centroid;
    if (!finiteNumber(center.x) || !finiteNumber(center.y)) {
        Vec2 sum;
        int n = 0;
        for (std::uint32_t q : body.ring)
            if (finiteNumber(bp.x[q]) && finiteNumber(bp.y[q])) {
                sum = sum + Vec2{bp.x[q], bp.y[q]};
                ++n;
            }
        center = n > 0 ? sum * (1.0f / static_cast<float>(n)) : body.lastFinite;
        body.centroid = center;
    }
    const float angle = finiteNumber(body.angle) ? body.angle : 0.0f;
    body.angle = angle;
    const float c = std::cos(angle), sn = std::sin(angle);
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const Vec2 p = center + rotate(body.restRing[i], c, sn);
        bp.x[body.ring[i]] = p.x;
        bp.y[body.ring[i]] = p.y;
        bp.vx[body.ring[i]] = bp.vy[body.ring[i]] = 0.0f;
    }
    for (std::size_t k = 0; k < body.interior.size(); ++k) {
        const Vec2 p = center + rotate(k < body.restInterior.size() ? body.restInterior[k] : Vec2{}, c, sn);
        bp.x[body.interior[k]] = p.x;
        bp.y[body.interior[k]] = p.y;
        bp.vx[body.interior[k]] = bp.vy[body.interior[k]] = 0.0f;
    }
    return true;
}

void refresh(RingBody& body, const BodyParticles& bp) {
    gatherPoly(body, bp);
    bestFit(body, bp, body.centroid, body.angle);
    if (finiteNumber(body.centroid.x) && finiteNumber(body.centroid.y))
        body.lastFinite = body.centroid;
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

namespace {

// Aparta el cuerpo entero del vidrio con el desplazamiento de su punto más hundido. false si no
// tocaba.
bool rigidOutOfDish(RingBody& body, BodyParticles& bp, const Dish& dish) {
    Vec2 shift;
    float worst = 0.0f;
    for (std::uint32_t s : body.ring) {
        const Vec2 p = pos(bp, s);
        const Vec2 d = dish.clampInside(p, bp.radius[s]) - p;
        if (dot(d, d) > worst) {
            worst = dot(d, d);
            shift = d;
        }
    }
    if (worst == 0.0f)
        return false;
    for (std::uint32_t s : body.ring)
        move(bp, s, shift);
    for (std::uint32_t s : body.interior)
        move(bp, s, shift);
    return true;
}

} // namespace

void confineToDish(RingBody& body, BodyParticles& bp, const Dish& dish) {
    if (!dish.active())
        return;
    // Una película, pared o teca no se amolda al vidrio: el cuerpo entero se aparta (proyectar
    // punto por punto aplastaba la punta de un ciliado que retrocede contra la pared y la membrana
    // quedaba plegada). La membrana plástica (ameba) sí se apoya y se amolda.
    if (body.membraneReservoirRate <= 0.0f) {
        rigidOutOfDish(body, bp, dish);
        return;
    }
    for (std::uint32_t s : body.ring) {
        const Vec2 p = pos(bp, s);
        const Vec2 q = dish.clampInside(p, bp.radius[s]);
        if (q.x != p.x || q.y != p.y) {
            bp.x[s] = q.x;
            bp.y[s] = q.y;
        }
    }
}

void pushIntoDish(RingBody& body, BodyParticles& bp, const Dish& dish) {
    if (!dish.active())
        return;
    for (int pass = 0; pass < 2; ++pass)
        if (!rigidOutOfDish(body, bp, dish))
            return;
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

void scaleToRestArea(RingBody& body, BodyParticles& bp) {
    const float a = std::abs(area(body, bp)), rest = std::abs(body.restArea);
    if (a <= 0.0f || rest <= 0.0f || std::abs(a - rest) < 0.02f * rest)
        return;
    // Gradual (≤ 5% por paso): un anillo retorcido tiene área aparente casi nula y escalarlo de
    // golpe lo haría explotar.
    const float k = std::clamp(std::sqrt(rest / a), 0.95f, 1.05f);
    Vec2 c;
    for (std::uint32_t s : body.ring)
        c = c + pos(bp, s);
    c = c * (1.0f / static_cast<float>(body.ring.size()));
    auto scale = [&](std::uint32_t s) {
        bp.x[s] = c.x + (bp.x[s] - c.x) * k;
        bp.y[s] = c.y + (bp.y[s] - c.y) * k;
    };
    for (std::uint32_t s : body.ring)
        scale(s);
    for (std::uint32_t s : body.interior)
        scale(s);
}

void setRestAreaAxial(RingBody& body, float newSignedArea) {
    if (body.restArea == 0.0f || newSignedArea * body.restArea <= 0.0f)
        return;
    const float s = newSignedArea / body.restArea; // el área es lineal en la escala de x
    for (Vec2& q : body.restRing)
        q.x *= s;
    const std::size_t n = body.restRing.size();
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    for (Vec2& q : body.restInterior)
        q.x *= s;
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
    body.baseCorticalTension = 5.0f;
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

void reshapeRest(RingBody& body, const BodyParticles& bp, float rx, float ry, float squareness, const Taper& taper) {
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
        const float r = std::pow(std::pow(std::abs(cp) / rx, squareness) + std::pow(std::abs(sp) / ry, squareness),
                                 -1.0f / squareness);
        const float x = r * cp;
        body.restRing[i] = {x, r * sp * taperFactor(taper, x, rx)};
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

void reshapeRestToOutline(RingBody& body, const BodyParticles& bp, const std::vector<Vec2>& outline, float scale) {
    const std::size_t n = body.ring.size();
    if (n < 3 || outline.size() < 3)
        return;
    Vec2 c;
    for (std::uint32_t s : body.ring)
        c = c + pos(bp, s);
    c = c * (1.0f / static_cast<float>(n));
    // Orientación: el giro del contorno que mejor calza con el anillo actual (cada punto del anillo
    // contra el punto más cercano del contorno), en pasos de 5°. Pasa pocas veces (división, quiste).
    constexpr int kTurns = 72;
    const std::size_t stride = std::max<std::size_t>(1, outline.size() / 160);
    float bestCost = std::numeric_limits<float>::max(), theta = 0.0f;
    for (int t = 0; t < kTurns; ++t) {
        const float a = 6.2831853f * static_cast<float>(t) / kTurns;
        const float co = std::cos(a), si = std::sin(a);
        float cost = 0.0f;
        for (std::uint32_t s : body.ring) {
            const Vec2 d = rotate(pos(bp, s) - c, co, -si);
            float nearest = std::numeric_limits<float>::max();
            for (std::size_t k = 0; k < outline.size(); k += stride) {
                const Vec2 e = outline[k] * scale - d;
                nearest = std::min(nearest, dot(e, e));
            }
            cost += nearest;
            if (cost >= bestCost)
                break;
        }
        if (cost < bestCost) {
            bestCost = cost;
            theta = a;
        }
    }
    const float co = std::cos(theta), si = std::sin(theta);
    // Mismo sentido de recorrido que el anillo; el primer punto, en la dirección que tiene ahora.
    float ringArea = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        ringArea += cross(pos(bp, body.ring[i]), pos(bp, body.ring[(i + 1) % n]));
    const bool reverse = ringArea < 0.0f;
    const Vec2 d0 = rotate(pos(bp, body.ring[0]) - c, co, -si);
    const float a0 = std::atan2(d0.y, d0.x);
    std::size_t k0 = 0;
    float bestGap = 10.0f;
    for (std::size_t k = 0; k < outline.size(); ++k) {
        float gap = std::abs(std::atan2(outline[k].y, outline[k].x) - a0);
        gap = std::min(gap, 6.2831853f - gap);
        if (gap < bestGap) {
            bestGap = gap;
            k0 = k;
        }
    }
    thread_local std::vector<Vec2> sampled;
    polygon::sampleClosed(outline, static_cast<int>(n), scale, polygon::arcFraction(outline, k0, reverse), reverse, sampled);
    // Centrado exacto en el promedio de los puntos (el centro que usa bestFit).
    Vec2 mean;
    for (const Vec2& q : sampled)
        mean = mean + q;
    mean = mean * (1.0f / static_cast<float>(n));
    body.restRing.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        body.restRing[i] = sampled[i] - mean;
    body.restEdge.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    body.restArea = polygon::signedArea(body.restRing.data(), static_cast<int>(n));
    body.centroid = c;
    body.angle = theta;
    for (std::size_t k = 0; k < body.interior.size(); ++k)
        body.restInterior[k] = toLocal(body, pos(bp, body.interior[k]));
    refresh(body, bp);
}

void reshapeRestEven(RingBody& body, const BodyParticles& bp, float rx, float ry, float squareness, const Taper& taper,
                     bool pinMinorAxis) {
    const std::size_t n = body.ring.size();
    if (n < 3)
        return;
    Vec2 c;
    for (std::uint32_t s : body.ring)
        c = c + pos(bp, s);
    c = c * (1.0f / static_cast<float>(n));
    float sxx = 0.0f, syy = 0.0f, sxy = 0.0f;
    for (std::uint32_t s : body.ring) {
        const Vec2 d = pos(bp, s) - c;
        sxx += d.x * d.x;
        syy += d.y * d.y;
        sxy += d.x * d.y;
    }
    const float theta = 0.5f * std::atan2(2.0f * sxy, sxx - syy);
    const float co = std::cos(theta), si = std::sin(theta);
    // Contorno meta denso (superelipse con el polo afinado), recorrido con el ángulo creciente.
    auto contour = [&](float phi) {
        const float cp = std::cos(phi), sp = std::sin(phi);
        const float r = std::pow(std::pow(std::abs(cp) / rx, squareness) + std::pow(std::abs(sp) / ry, squareness),
                                 -1.0f / squareness);
        const float x = r * cp;
        return Vec2{x, r * sp * taperFactor(taper, x, rx)};
    };
    const std::size_t m = 8 * n;
    // Buffers por hilo (se llama en cada paso mientras la célula cambia de forma): sin asignar.
    thread_local std::vector<Vec2> dense;
    thread_local std::vector<float> arc;
    dense.resize(m + 1);
    arc.assign(m + 1, 0.0f);
    for (std::size_t k = 0; k <= m; ++k) {
        dense[k] = contour(6.2831853f * static_cast<float>(k) / static_cast<float>(m));
        if (k > 0)
            arc[k] = arc[k - 1] + length(dense[k] - dense[k - 1]);
    }
    const float total = arc[m];
    // El primer punto conserva su dirección; el sentido de recorrido es el del anillo.
    const Vec2 d0 = rotate(pos(bp, body.ring[0]) - c, co, -si);
    float phi0 = std::atan2(d0.y, d0.x);
    if (phi0 < 0.0f)
        phi0 += 6.2831853f;
    const std::size_t k0 = std::min(m, static_cast<std::size_t>(phi0 / 6.2831853f * static_cast<float>(m)));
    float s0 = arc[k0];
    if (pinMinorAxis) { // el reparto pasa por φ = π/2 (y, con n par, por 3π/2)
        const float spacing = total / static_cast<float>(n);
        const float waist = arc[m / 4];
        s0 = waist + spacing * std::round((s0 - waist) / spacing);
    }
    float twiceArea = 0.0f; // sentido del anillo, de las posiciones actuales
    for (std::size_t i = 0; i < n; ++i)
        twiceArea += cross(pos(bp, body.ring[i]), pos(bp, body.ring[(i + 1) % n]));
    const float dir = twiceArea >= 0.0f ? 1.0f : -1.0f;
    body.restRing.resize(n);
    body.restEdge.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        float s = std::fmod(s0 + dir * total * static_cast<float>(i) / static_cast<float>(n), total);
        if (s < 0.0f)
            s += total;
        std::size_t k = static_cast<std::size_t>(std::lower_bound(arc.begin(), arc.end(), s) - arc.begin());
        k = std::clamp<std::size_t>(k, 1, m);
        const float seg = arc[k] - arc[k - 1];
        const float t = seg > 1e-9f ? (s - arc[k - 1]) / seg : 0.0f;
        body.restRing[i] = dense[k - 1] + (dense[k] - dense[k - 1]) * t;
    }
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    body.restArea = polygon::signedArea(body.restRing.data(), static_cast<int>(n));
    body.centroid = c;
    body.angle = theta;
    for (std::size_t q = 0; q < body.interior.size(); ++q)
        body.restInterior[q] = toLocal(body, pos(bp, body.interior[q]));
    refresh(body, bp);
}

void resizeRing(RingBody& body, const BodyParticles& bp, std::vector<std::uint32_t> ring) {
    body.ring = std::move(ring);
    rebuildRest(body, bp, true);
}

bool untangle(RingBody& body, BodyParticles& bp) {
    gatherPoly(body, bp);
    const int n = static_cast<int>(body.poly.size());
    if (!polygon::selfIntersects(body.poly.data(), n))
        return false;
    // 2-opt: invertir el tramo entre dos aristas que se cruzan quita ese cruce sin mover ningún
    // punto (solo cambia con quién se conecta cada uno), así no hay saltos.
    auto crosses = [](Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
        return cross(b - a, c - a) * cross(b - a, d - a) < 0.0f && cross(d - c, a - c) * cross(d - c, b - c) < 0.0f;
    };
    std::vector<std::uint32_t> ring = body.ring;
    for (int pass = 0; pass < 64; ++pass) {
        bool found = false;
        for (int i = 0; i < n && !found; ++i) {
            for (int j = i + 2; j < n && !found; ++j) {
                if (i == 0 && j == n - 1)
                    continue;
                const Vec2 a = pos(bp, ring[static_cast<std::size_t>(i)]), b = pos(bp, ring[static_cast<std::size_t>(i + 1)]);
                const Vec2 c = pos(bp, ring[static_cast<std::size_t>(j)]), d = pos(bp, ring[static_cast<std::size_t>((j + 1) % n)]);
                if (crosses(a, b, c, d)) {
                    std::reverse(ring.begin() + i + 1, ring.begin() + j + 1);
                    found = true;
                }
            }
        }
        if (!found)
            break;
    }
    resizeRing(body, bp, std::move(ring));
    return true;
}

float membraneExcess(const RingBody& body, const BodyParticles& bp) {
    const float circle = 2.0f * std::sqrt(3.14159265f * std::abs(body.restArea));
    return circle > 0.0f ? perimeter(body, bp) / circle : 1.0f;
}

void updateMembraneTension(RingBody& body, const BodyParticles& bp) {
    if (body.membraneReservoirRate <= 0.0f)
        return;
    const float excess = membraneExcess(body, bp);
    const float over = std::max(1.0f, excess / 2.0f);
    body.corticalTension = body.baseCorticalTension * over * over * over * over;
}

bool remesh(RingBody& body, BodyParticles& bp, float spacing) {
    if (body.membraneReservoirRate <= 0.0f)
        return false;
    const bool reservoirLeft = membraneExcess(body, bp) < 3.0f;
    const std::size_t n = body.ring.size();
    std::vector<std::uint32_t> ring;
    ring.reserve(n + 8);
    bool changed = false;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t a = body.ring[i], b = body.ring[(i + 1) % n];
        ring.push_back(a);
        const Vec2 pa = pos(bp, a), pb = pos(bp, b);
        // Tope de seguridad: una membrana no crece sin límite aunque algo la estire.
        if (reservoirLeft && length(pb - pa) > 1.6f * spacing && n + ring.size() - i < 2000) {
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
