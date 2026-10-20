#include "bio/silhouette.hpp"

#include "bio/species.hpp"
#include "core/units.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

constexpr float kGridStep = 0.25f;    // resolución del campo [µm]: menor que la punta más fina (2 µm)
constexpr float kDenseSpacing = 0.5f; // separación del contorno denso [µm]
constexpr int kAxisSamples = 40;      // tramos del eje de cada prolongación

// Mínimo suave (polinomial): une dos campos de distancia con un redondeo de radio ~k.
float smoothMin(float a, float b, float k) {
    if (k <= 0.0f)
        return std::min(a, b);
    const float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return b + (a - b) * h - k * h * (1.0f - h);
}

float taperFactorAt(const CellSpec& s, float x) { return taperFactor(s.taper, x, s.radiusX); }

// Distancia aproximada (con signo) al borde de la superelipse afinada del cuerpo: exacta en el borde,
// que es lo único que se usa (el nivel cero), y con la escala del semieje menor alrededor.
float bodyDistance(const CellSpec& s, Vec2 q) {
    const float y = q.y / taperFactorAt(s, q.x);
    const float p = s.squareness;
    const float f = std::pow(std::pow(std::abs(q.x) / s.radiusX, p) + std::pow(std::abs(y) / s.radiusY, p), 1.0f / p);
    return (f - 1.0f) * std::min(s.radiusX, s.radiusY);
}

// Punto del borde del cuerpo en la dirección `a` desde su centro.
Vec2 bodyEdge(const CellSpec& s, float a) {
    // Por bisección sobre el radio (el afinamiento hace que la forma no sea una superelipse exacta).
    const Vec2 d = {std::cos(a), std::sin(a)};
    float lo = 0.0f, hi = 2.0f * std::max(s.radiusX, s.radiusY);
    for (int i = 0; i < 40; ++i) {
        const float mid = 0.5f * (lo + hi);
        (bodyDistance(s, d * mid) < 0.0f ? lo : hi) = mid;
    }
    return d * lo;
}

struct Capsule {
    std::vector<Vec2> axis;
    std::vector<float> half; // medio ancho en cada punto del eje
    float smooth;            // radio del redondeo con el cuerpo
    Vec2 lo, hi;             // caja (con margen)
};

float capsuleDistance(const Capsule& c, Vec2 q) {
    float best = 1e9f;
    for (std::size_t i = 0; i + 1 < c.axis.size(); ++i) {
        const Vec2 a = c.axis[i], e = c.axis[i + 1] - a;
        const float len2 = dot(e, e);
        const float t = len2 > 0.0f ? std::clamp(dot(q - a, e) / len2, 0.0f, 1.0f) : 0.0f;
        const float r = c.half[i] + (c.half[i + 1] - c.half[i]) * t;
        best = std::min(best, length(q - (a + e * t)) - r);
    }
    return best;
}

// Contorno de nivel cero por marching squares: las aristas cruzadas de la grilla se encadenan en
// lazos y se devuelve el de mayor área.
std::vector<Vec2> zeroContour(const std::vector<float>& v, int nx, int ny, Vec2 origin, float h) {
    auto at = [&](int i, int j) { return v[static_cast<std::size_t>(j) * nx + i]; };
    auto node = [&](int i, int j) { return Vec2{origin.x + static_cast<float>(i) * h, origin.y + static_cast<float>(j) * h}; };
    // Arista horizontal (i,j)–(i+1,j): id par; vertical (i,j)–(i,j+1): id impar.
    auto hEdge = [&](int i, int j) { return 2ll * (static_cast<long long>(j) * nx + i); };
    auto vEdge = [&](int i, int j) { return 2ll * (static_cast<long long>(j) * nx + i) + 1; };
    std::unordered_map<long long, Vec2> point;
    std::unordered_map<long long, std::vector<long long>> links;
    auto cross = [&](long long id, Vec2 a, Vec2 b, float va, float vb) {
        if (!point.count(id)) {
            const float t = va / (va - vb);
            point[id] = a + (b - a) * t;
        }
    };
    auto link = [&](long long a, long long b) {
        links[a].push_back(b);
        links[b].push_back(a);
    };
    for (int j = 0; j + 1 < ny; ++j) {
        for (int i = 0; i + 1 < nx; ++i) {
            const float v0 = at(i, j), v1 = at(i + 1, j), v2 = at(i + 1, j + 1), v3 = at(i, j + 1);
            const int code = (v0 < 0.0f ? 1 : 0) | (v1 < 0.0f ? 2 : 0) | (v2 < 0.0f ? 4 : 0) | (v3 < 0.0f ? 8 : 0);
            if (code == 0 || code == 15)
                continue;
            const long long eb = hEdge(i, j), er = vEdge(i + 1, j), et = hEdge(i, j + 1), el = vEdge(i, j);
            if ((v0 < 0.0f) != (v1 < 0.0f))
                cross(eb, node(i, j), node(i + 1, j), v0, v1);
            if ((v1 < 0.0f) != (v2 < 0.0f))
                cross(er, node(i + 1, j), node(i + 1, j + 1), v1, v2);
            if ((v3 < 0.0f) != (v2 < 0.0f))
                cross(et, node(i, j + 1), node(i + 1, j + 1), v3, v2);
            if ((v0 < 0.0f) != (v3 < 0.0f))
                cross(el, node(i, j), node(i, j + 1), v0, v3);
            const float center = 0.25f * (v0 + v1 + v2 + v3);
            switch (code) {
            case 1: case 14: link(el, eb); break;
            case 2: case 13: link(eb, er); break;
            case 3: case 12: link(el, er); break;
            case 4: case 11: link(er, et); break;
            case 6: case 9: link(eb, et); break;
            case 7: case 8: link(el, et); break;
            case 5: // esquinas opuestas adentro: el centro decide si se unen
                if (center < 0.0f) { link(el, et); link(eb, er); } else { link(el, eb); link(er, et); }
                break;
            case 10:
                if (center < 0.0f) { link(el, eb); link(er, et); } else { link(el, et); link(eb, er); }
                break;
            default: break;
            }
        }
    }
    // Recorrido de los lazos en orden de id (determinista).
    std::vector<long long> ids;
    ids.reserve(links.size());
    for (const auto& [id, l] : links)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    std::unordered_map<long long, bool> seen;
    std::vector<Vec2> best;
    float bestArea = 0.0f;
    for (long long start : ids) {
        if (seen[start])
            continue;
        std::vector<Vec2> loop;
        long long prev = -1, cur = start;
        while (cur >= 0 && !seen[cur]) {
            seen[cur] = true;
            loop.push_back(point[cur]);
            long long next = -1;
            for (long long n : links[cur])
                if (n != prev && !seen[n]) {
                    next = n;
                    break;
                }
            prev = cur;
            cur = next;
        }
        if (loop.size() < 3)
            continue;
        const float a = std::abs(polygon::signedArea(loop.data(), static_cast<int>(loop.size())));
        if (a > bestArea) {
            bestArea = a;
            best = std::move(loop);
        }
    }
    return best;
}

} // namespace

float outlinePerimeter(const std::vector<Vec2>& outline) { return polygon::perimeter(outline); }

void sampleOutline(const std::vector<Vec2>& dense, int n, float scale, float start, bool reverse, std::vector<Vec2>& out) {
    polygon::sampleClosed(dense, n, scale, start, reverse, out);
}

Silhouette buildSilhouette(const CellSpec& body, const std::vector<ProcessSpec>& processes) {
    Silhouette sil;
    std::vector<Capsule> caps;
    for (const ProcessSpec& p : processes) {
        if (!p.inBody)
            continue;
        for (int k = 0; k < p.count; ++k) {
            const float a = processAngle(p, k);
            const Vec2 dir = {std::cos(a), std::sin(a)}, side = {-dir.y, dir.x};
            // La base arranca medio ancho adentro del cuerpo: el redondeo la une sin escalón.
            const Vec2 base = bodyEdge(body, a) - dir * (0.5f * p.baseWidth);
            Capsule c;
            c.smooth = p.fillet > 0.0f ? p.fillet : 0.5f * p.baseWidth;
            c.lo = c.hi = base;
            // El largo se mide desde el borde del cuerpo hasta la punta redondeada: el eje, que arranca
            // escondido en el cuerpo, se estira para que la punta llegue ahí.
            const float reach = p.length + 0.5f * p.baseWidth - 0.5f * p.tipWidth;
            const float stretch = p.length > 0.0f ? reach / p.length : 1.0f;
            for (int i = 0; i <= kAxisSamples; ++i) {
                const float s = static_cast<float>(i) / kAxisSamples;
                const Vec2 ax = processAxisPoint(p, s, stretch);
                const Vec2 q = base + dir * ax.x + side * ax.y;
                const float r = 0.5f * processWidth(p, s, 1.0f);
                c.axis.push_back(q);
                c.half.push_back(r);
                c.lo = {std::min(c.lo.x, q.x - r), std::min(c.lo.y, q.y - r)};
                c.hi = {std::max(c.hi.x, q.x + r), std::max(c.hi.y, q.y + r)};
            }
            const float margin = c.smooth + 2.0f * kGridStep;
            c.lo = c.lo - Vec2{margin, margin};
            c.hi = c.hi + Vec2{margin, margin};
            caps.push_back(std::move(c));
        }
    }
    if (caps.empty())
        return sil;

    // Grilla que cubre el cuerpo y todas las prolongaciones.
    Vec2 lo = {-body.radiusX, -body.radiusY}, hi = {body.radiusX, body.radiusY};
    for (const Capsule& c : caps) {
        lo = {std::min(lo.x, c.lo.x), std::min(lo.y, c.lo.y)};
        hi = {std::max(hi.x, c.hi.x), std::max(hi.y, c.hi.y)};
    }
    lo = lo - Vec2{4.0f * kGridStep, 4.0f * kGridStep};
    hi = hi + Vec2{4.0f * kGridStep, 4.0f * kGridStep};
    const int nx = static_cast<int>(std::ceil((hi.x - lo.x) / kGridStep)) + 1;
    const int ny = static_cast<int>(std::ceil((hi.y - lo.y) / kGridStep)) + 1;
    std::vector<float> field(static_cast<std::size_t>(nx) * ny);
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const Vec2 q = {lo.x + static_cast<float>(i) * kGridStep, lo.y + static_cast<float>(j) * kGridStep};
            float d = bodyDistance(body, q);
            for (const Capsule& c : caps)
                if (q.x >= c.lo.x && q.x <= c.hi.x && q.y >= c.lo.y && q.y <= c.hi.y)
                    d = smoothMin(d, capsuleDistance(c, q), c.smooth);
            field[static_cast<std::size_t>(j) * nx + i] = d;
        }
    }
    std::vector<Vec2> loop = zeroContour(field, nx, ny, lo, kGridStep);
    if (loop.size() < 3)
        return sil;
    if (polygon::signedArea(loop.data(), static_cast<int>(loop.size())) < 0.0f)
        std::reverse(loop.begin(), loop.end());

    const int dense = std::max(64, static_cast<int>(outlinePerimeter(loop) / kDenseSpacing));
    // Arranca en el polo anterior del cuerpo (el punto más cercano a +x desde su centro, el origen).
    std::size_t first = 0;
    float bestDot = -1e9f;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const float score = loop[i].x - 4.0f * std::abs(loop[i].y); // sobre el eje, lo más adelante posible
        if (score > bestDot) {
            bestDot = score;
            first = i;
        }
    }
    std::rotate(loop.begin(), loop.begin() + static_cast<std::ptrdiff_t>(first), loop.end());
    sampleOutline(loop, dense, 1.0f, 0.0f, false, sil.outline);
    // Centrado en el promedio de los puntos del contorno (repartidos parejo por longitud de arco): el
    // cuerpo blando toma ese promedio como su centro (soft_body::bestFit), no el centroide de área. Con
    // cuernos los dos se separan mucho (los cuernos tienen poca área y mucho perímetro).
    Vec2 mean;
    for (const Vec2& p : sil.outline)
        mean = mean + p;
    mean = mean * (1.0f / static_cast<float>(sil.outline.size()));
    for (Vec2& p : sil.outline)
        p = p - mean;
    const Vec2 bodyCenter = Vec2{0.0f, 0.0f} - mean;
    sil.area = std::abs(polygon::signedArea(sil.outline.data(), static_cast<int>(sil.outline.size())));
    sil.bodyCenter = bodyCenter;
    sil.areaFactor = sil.area / (units::kPi * body.radiusX * body.radiusY);
    return sil;
}
