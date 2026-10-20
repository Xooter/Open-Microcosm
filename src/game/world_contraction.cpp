// Contracción del cuerpo entero (Stentor): mecanismo de docs/plan_mecanicas.md (punto 8).
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
// Las células con contracción siempre tienen especie (como en game/world_animal.cpp).
const SpeciesInfo& infoOf(const entt::registry& reg, entt::entity e) {
    static const SpeciesInfo kNone;
    const Species* sp = reg.try_get<Species>(e);
    return sp ? speciesInfo(sp->id) : kNone;
}
// La contracción real dura ~5–10 ms, menos que un paso: la forma de reposo salta a la contraída y la
// memoria de forma muy alta lleva ahí al cuerpo en unos pocos pasos.
constexpr float kContractRate = 40.0f; // [1/s]
// Exponente del perfil por omisión. Trompeta: la mitad posterior queda fina (el "pedúnculo", que es la
// misma célula) y la anterior se abre hasta el disco oral. Campana: lados convexos, ancha casi enseguida.
constexpr float kTrumpetFlare = 3.0f;
constexpr float kBellFlare = 0.5f;
// El labio del peristoma sobresale un poco del cuerpo de la campana (en el último 15 % del largo).
constexpr float kBellLip = 0.12f;
// El disco oral es casi plano; apenas abombado, así su centro es el polo anterior.
constexpr float kDiscDome = 0.05f;

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

float restHalfLength(const RingBody& body) {
    float l = 0.0f;
    for (const Vec2& q : body.restRing)
        l = std::max(l, std::abs(q.x));
    return l;
}

// Volumen del sólido de revolución del contorno alrededor de su eje (x local) [µm³].
float revolvedVolume(const std::vector<Vec2>& ring) {
    float v = 0.0f;
    const std::size_t n = ring.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 a = ring[i], b = ring[(i + 1) % n];
        v += 0.5f * units::kPi * 0.5f * (a.y * a.y + b.y * b.y) * std::abs(b.x - a.x); // cada mitad, una vez
    }
    return v;
}

// Contorno denso de la trompeta o la campana extendida, antihorario desde el centro del disco oral: disco, borde
// superior hasta el pie, pie redondeado, borde inferior y de nuevo el disco.
void trumpetOutline(float halfLength, float oral, float foot, float flare, float lip, std::vector<Vec2>& out) {
    constexpr int kDisc = 16, kSide = 128, kCap = 16;
    auto radius = [&](float u) {
        const float r = oral * (foot + (1.0f - foot) * std::pow(0.5f * (u + 1.0f), flare));
        return r * (1.0f + lip * smoothstep(std::clamp((u - 0.7f) / 0.3f, 0.0f, 1.0f)));
    };
    auto disc = [&](float y) { return halfLength - kDiscDome * oral * (y / oral) * (y / oral); };
    out.clear();
    for (int k = 0; k < kDisc; ++k) {
        const float y = oral * static_cast<float>(k) / kDisc;
        out.push_back({disc(y), y});
    }
    for (int k = 0; k < kSide; ++k) {
        const float u = 1.0f - 2.0f * static_cast<float>(k) / kSide;
        out.push_back({std::min(halfLength * u, disc(radius(u))), radius(u)});
    }
    const float r = radius(-1.0f);
    for (int k = 0; k < kCap; ++k) {
        const float a = 0.5f * units::kPi + units::kPi * static_cast<float>(k) / kCap;
        out.push_back({-halfLength + r * std::cos(a), r * std::sin(a)});
    }
    for (int k = 0; k < kSide; ++k) {
        const float u = -1.0f + 2.0f * static_cast<float>(k) / kSide;
        out.push_back({std::min(halfLength * u, disc(radius(u))), -radius(u)});
    }
    for (int k = 0; k < kDisc; ++k) {
        const float y = -oral * (1.0f - static_cast<float>(k) / kDisc);
        out.push_back({disc(y), y});
    }
    // El borde del peristoma es redondeado, no una arista: unas pasadas de suavizado sobre el contorno.
    thread_local std::vector<Vec2> tmp;
    const std::size_t m = out.size();
    for (int pass = 0; pass < 6; ++pass) {
        tmp = out;
        for (std::size_t i = 0; i < m; ++i)
            out[i] = tmp[(i + m - 1) % m] * 0.25f + tmp[i] * 0.5f + tmp[(i + 1) % m] * 0.25f;
    }
}

// Formas de reposo contraída (la actual) y extendida de la célula, punto a punto. La extendida reparte
// los puntos por longitud de arco desde el polo anterior, en el mismo sentido que el anillo.
void buildShapes(Contraction& c, const RingBody& body, const ContractionSpec& spec) {
    c.contracted = body.restRing;
    const std::size_t n = c.contracted.size();
    c.extended.resize(n);
    float minX = 1e9f, maxX = -1e9f;
    std::size_t front = 0;
    for (std::size_t i = 0; i < n; ++i) {
        minX = std::min(minX, c.contracted[i].x);
        if (c.contracted[i].x > maxX) {
            maxX = c.contracted[i].x;
            front = i;
        }
    }
    const float halfContracted = 0.5f * (maxX - minX);
    if (spec.profile == ContractionProfile::Uniform) {
        for (std::size_t i = 0; i < n; ++i)
            c.extended[i] = {c.contracted[i].x * spec.extension, c.contracted[i].y / spec.extension};
        return;
    }
    // Trompeta y campana: el radio de la boca conserva el volumen de la contraída. Con r(u) = R·(f + (1−f)·v^p),
    // v = (u+1)/2, el volumen es π·R²·2L·E con E = ⟨(f + (1−f)·v^p)²⟩.
    const float halfLength = halfContracted * spec.extension;
    const float f = spec.footWidth;
    const float p = spec.flare > 0.0f ? spec.flare : spec.profile == ContractionProfile::Bell ? kBellFlare : kTrumpetFlare;
    const float meanSquare = f * f + 2.0f * f * (1.0f - f) / (p + 1.0f) + (1.0f - f) * (1.0f - f) / (2.0f * p + 1.0f);
    const float oral = std::sqrt(revolvedVolume(c.contracted) / (units::kPi * 2.0f * halfLength * meanSquare));
    thread_local std::vector<Vec2> outline;
    thread_local std::vector<float> cum;
    // El labio agrega ~2 % de volumen: se desprecia.
    trumpetOutline(halfLength, oral, f, p, spec.profile == ContractionProfile::Bell ? kBellLip : 0.0f, outline);
    if (polygon::signedArea(c.contracted.data(), static_cast<int>(n)) < 0.0f)
        for (Vec2& q : outline) // anillo horario: el contorno también
            q.y = -q.y;
    const std::size_t m = outline.size();
    cum.assign(m + 1, 0.0f);
    for (std::size_t k = 0; k < m; ++k)
        cum[k + 1] = cum[k] + length(outline[(k + 1) % m] - outline[k]);
    float ringLength = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        ringLength += length(c.contracted[(i + 1) % n] - c.contracted[i]);
    float s = 0.0f;
    std::size_t k = 0;
    for (std::size_t j = 0; j < n; ++j) {
        const std::size_t i = (front + j) % n;
        const float target = s / ringLength * cum[m];
        while (k + 1 < m && cum[k + 1] < target)
            ++k;
        const float seg = cum[k + 1] - cum[k];
        const float t = seg > 1e-6f ? (target - cum[k]) / seg : 0.0f;
        c.extended[i] = outline[k] + (outline[(k + 1) % m] - outline[k]) * t;
        s += length(c.contracted[(i + 1) % n] - c.contracted[i]);
    }
}

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

// Lleva la forma de reposo a la extensión `s` (0 contraída, 1 extendida) con el área de reposo actual, y
// acomoda los organelos: cada uno conserva la fracción de volumen a lo largo del eje y su lugar relativo
// a lo ancho (el citoplasma fluye al estirarse; si no, quedarían fuera del pedúnculo fino).
void applyShape(RingBody& body, Contraction& c, float s) {
    const std::size_t n = body.restRing.size();
    thread_local std::vector<Vec2> before;
    thread_local AxialProfile from, to;
    before = body.restRing;
    for (std::size_t i = 0; i < n; ++i)
        body.restRing[i] = c.contracted[i] + (c.extended[i] - c.contracted[i]) * s;
    const float a = std::abs(polygon::signedArea(body.restRing.data(), static_cast<int>(n)));
    const float a0 = std::abs(polygon::signedArea(c.contracted.data(), static_cast<int>(n)));
    // El área de reposo (la de la biomasa por la de la silueta) cambia con la forma en el mismo paso.
    const float scale = a0 > 0.0f ? a / a0 : 1.0f;
    const float area = std::abs(body.restArea) * scale / std::max(1e-6f, c.areaScale);
    const float g = a > 0.0f ? std::sqrt(area / a) : 1.0f;
    for (Vec2& q : body.restRing)
        q = q * g;
    body.restArea = body.restArea >= 0.0f ? area : -area;
    c.areaScale = scale;
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    // La forma de reposo va centrada en el promedio de sus puntos, que es el centro de soft_body::bestFit:
    // la campana y la trompeta no son simétricas y, corridas, la memoria de forma empujaría al cuerpo
    // entero (se despegaba del tallo).
    Vec2 mean;
    for (const Vec2& q : body.restRing)
        mean = mean + q;
    mean = mean * (1.0f / static_cast<float>(n));
    for (Vec2& q : body.restRing)
        q = q - mean;
    if (body.restInterior.empty())
        return;
    axialProfile(before, from);
    axialProfile(body.restRing, to);
    for (Vec2& q : body.restInterior) {
        const int b0 = binOf(from, q.x);
        const float lo0 = from.lo[static_cast<std::size_t>(b0)], hi0 = from.hi[static_cast<std::size_t>(b0)];
        const float x = xAtFraction(to, fractionAt(from, q.x));
        const int b1 = binOf(to, x);
        const float lo1 = to.lo[static_cast<std::size_t>(b1)], hi1 = to.hi[static_cast<std::size_t>(b1)];
        const float half0 = 0.5f * (hi0 - lo0), half1 = 0.5f * (hi1 - lo1);
        const float rel = half0 > 1e-3f ? std::clamp((q.y - 0.5f * (lo0 + hi0)) / half0, -0.85f, 0.85f) : 0.0f;
        q = {x, 0.5f * (lo1 + hi1) + rel * half1}; // `to` ya es del anillo centrado
    }
}
} // namespace

void World::updateContraction(entt::entity e, RingBody& body, bool trigger, float dt) {
    PROFILE_SCOPE("World::updateContraction");
    const SpeciesInfo& info = infoOf(registry_, e);
    const ContractionSpec& spec = info.contraction;
    if (spec.extension <= 1.0f)
        return;
    // Sin el componente es recién sembrada: arranca extendida, como se la ve en el agua. Las hijas de una
    // división lo traen puesto (contraídas) y se extienden de a poco.
    const bool fresh = !registry_.all_of<Contraction>(e);
    Contraction& c = registry_.get_or_emplace<Contraction>(e);
    // Dividiéndose o en quiste, la forma la llevan la división o la pared (desde la contraída).
    if (registry_.any_of<Dividing, Cyst>(e)) {
        c.state = 1.0f;
        c.stretch = 1.0f;
        c.contracted.clear(); // al salir, la forma de reposo es la contraída: se rehacen
        return;
    }
    const bool anchored = registry_.all_of<Anchored>(e);
    const float cap = anchored ? 1.0f : spec.swimmingExtension; // suelta nada en forma de pera
    bool rebuilt = false;
    if (c.contracted.size() != body.restRing.size()) {
        buildShapes(c, body, spec);
        c.state = fresh ? 1.0f - cap : 1.0f;
        c.stretch = 1.0f;
        rebuilt = true;
    }
    // El zooide de un peritrico se contrae con su espasmonema (también en las contracciones espontáneas
    // del tallo y, en Zoothamnium, cuando se contrae la colonia entera).
    bool fire = trigger;
    if (const Anchored* an = registry_.try_get<Anchored>(e); !fire && an && registry_.valid(an->rod) && c.state < 0.5f)
        if (const Stalk* st = registry_.try_get<Stalk>(an->rod); st && st->rod.contracted > 0.97f)
            fire = true;
    // Contracciones espontáneas (sin estímulo), solo extendida.
    if (!fire && spec.spontaneousPerMinute > 0.0f && c.state < 1.0f - 0.95f * cap) {
        Rng rng(config_.seed, rng_stream::kContraction, body.id, step_);
        fire = rng.uniform() < spec.spontaneousPerMinute / 60.0f * dt;
    }
    const float before = c.state;
    if (fire) {
        c.state = 1.0f;
    } else {
        const float target = 1.0f - cap, rate = dt / spec.extensionSeconds;
        c.state = c.state > target ? std::max(target, c.state - rate) : std::min(target, c.state + rate);
    }
    if (!fire && c.state == before && !rebuilt)
        return; // quieta en su extensión
    // Re-extensión suave; la contracción, de golpe.
    const float s = smoothstep(1.0f - c.state);
    const float stretch = 1.0f + (spec.extension - 1.0f) * s;
    if (fire && c.stretch > 1.05f) {
        // Los dos extremos se acercan al centro de golpe y arrastran el agua.
        const float half = restHalfLength(body);
        const float shortening = half * (1.0f - stretch / c.stretch);
        contractionKicks_.push_back({body.centroid, bodyHeading(body), half, shortening / dt,
                                     std::max(6.0f, 0.5f * std::abs(body.restArea) / (units::kPi * half))});
    }
    applyShape(body, c, s);
    c.stretch = stretch;
    const CellShape* shape = registry_.try_get<CellShape>(e);
    body.shapeRate = c.state > 0.9f ? kContractRate : shape ? shape->shapeRate : body.shapeRate;
}

float World::contractionAreaScale(entt::entity e) const {
    const Contraction* c = registry_.try_get<Contraction>(e);
    return c ? c->areaScale : 1.0f;
}

void World::contractForDivision(entt::entity e, RingBody& body) {
    // Simplificación: la célula se divide con la forma contraída de la especie (el surco de la división
    // no llega a cerrar en una célula de casi 1 mm). Stentor real se divide extendido.
    Contraction* c = registry_.try_get<Contraction>(e);
    if (!c || c->stretch <= 1.0f || c->contracted.size() != body.restRing.size())
        return;
    applyShape(body, *c, 0.0f);
    c->state = 1.0f;
    c->stretch = 1.0f;
}

void World::applyContractionKicks() {
    // Fuerza que mueve el agua a ~`speed` en cada extremo, hacia el centro (par de fuerzas opuestas:
    // fuerza neta cero, como todo nadador a bajo Reynolds).
    for (const ContractionKick& kick : contractionKicks_) {
        const float response = fluid_.viscosity / (kick.radius * kick.radius) + fluid_.brinkmanFriction();
        for (float side : {1.0f, -1.0f}) {
            const Vec2 pole = kick.center + kick.axis * (side * kick.halfLength);
            fluid_.addForce(pole, kick.axis * (-side * kick.speed * response), kick.radius);
        }
    }
    contractionKicks_.clear();
}
