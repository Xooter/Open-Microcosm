// Sedimento inorgánico: partículas minerales sueltas (arcilla, limo de cuarzo, mica, óxidos de
// hierro, azufre, frústulas vacías) y granos de arena fijos. Nada de esto está vivo: no se
// descompone, no se come y no cuenta como biomasa, pero el agua y las células lo sienten.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Tamaños típicos [µm de radio].
float mineralRadius(MineralKind kind, Rng& rng) {
    switch (kind) {
    case MineralKind::Clay: return rng.uniform(0.3f, 1.2f);
    case MineralKind::Quartz: return rng.uniform(2.5f, 15.0f);
    case MineralKind::Mica: return rng.uniform(3.0f, 10.0f);
    case MineralKind::IronOxide: return rng.uniform(1.5f, 6.0f);
    case MineralKind::Sulfur: return rng.uniform(0.8f, 3.0f);
    case MineralKind::Frustule: return rng.uniform(18.0f, 28.0f); // medio largo de Navicula
    case MineralKind::Count: break;
    }
    return 1.0f;
}

} // namespace

const char* mineralName(MineralKind kind) {
    switch (kind) {
    case MineralKind::Clay: return "Arcilla";
    case MineralKind::Quartz: return "Limo de cuarzo";
    case MineralKind::Mica: return "Mica";
    case MineralKind::IronOxide: return "Oxido de hierro";
    case MineralKind::Sulfur: return "Azufre";
    case MineralKind::Frustule: return "Frustula vacia";
    case MineralKind::Count: break;
    }
    return "";
}

void World::spawnMineral(MineralKind kind, Vec2 at, Rng& rng) {
    const float r = mineralRadius(kind, rng);
    const Vec2 p = dish_.clampInside(at, r);
    ambient_.push(ParticleKind::Mineral, wrapCoord(p.x, width_), wrapCoord(p.y, height_), r, 0.0f, kind,
                  rng.uniform(0.0f, 2.0f * units::kPi));
}

void World::spawnSediment(Vec2 at, int count, Rng& rng) {
    // Mezcla de un barro típico: sobre todo arcilla y limo, algo de mica y óxidos.
    for (int k = 0; k < count; ++k) {
        const float u = rng.uniform();
        const MineralKind kind = u < 0.55f ? MineralKind::Clay
                                 : u < 0.85f ? MineralKind::Quartz
                                 : u < 0.93f ? MineralKind::IronOxide
                                             : MineralKind::Mica;
        const float a = rng.uniform(0.0f, 2.0f * units::kPi), d = 25.0f * std::sqrt(rng.uniform());
        spawnMineral(kind, {at.x + d * std::cos(a), at.y + d * std::sin(a)}, rng);
    }
}

bool World::spawnRock(Vec2 at, float radius, MineralKind kind, Rng& rng) {
    PROFILE_SCOPE("World::spawnRock");
    at = dish_.clampInside(at, radius * 1.3f + 4.0f);
    // No aparece encima de nada: ni de una célula ni de otro grano.
    for (auto [e, b] : registry_.view<RingBody>().each()) {
        const Vec2 c = (b.aabbMin + b.aabbMax) * 0.5f;
        const Vec2 half = (b.aabbMax - b.aabbMin) * 0.5f;
        const float dx = std::abs(minImage(c.x - at.x, width_)), dy = std::abs(minImage(c.y - at.y, height_));
        if (dx < half.x + 1.3f * radius + 4.0f && dy < half.y + 1.3f * radius + 4.0f)
            return false;
    }
    // Contorno de grano redondeado-subangular: radio con armónicos bajos al azar (siempre
    // estrellado desde el centro, así el abanico de triángulos del render es válido).
    const float a2 = rng.uniform(0.05f, 0.18f), a3 = rng.uniform(0.0f, 0.08f), a5 = rng.uniform(0.0f, 0.04f);
    const float p2 = rng.uniform(0.0f, 6.2831853f), p3 = rng.uniform(0.0f, 6.2831853f), p5 = rng.uniform(0.0f, 6.2831853f);
    auto contour = [&](float t) {
        const float rr = radius * (1.0f + a2 * std::cos(2.0f * t + p2) + a3 * std::cos(3.0f * t + p3) +
                                   a5 * std::cos(5.0f * t + p5));
        return Vec2{rr * std::cos(t), rr * std::sin(t)};
    };
    float perimeter = 0.0f;
    for (int k = 0; k < 256; ++k)
        perimeter += length(contour(6.2831853f * (k + 1) / 256.0f) - contour(6.2831853f * k / 256.0f));
    const int n = std::max(24, static_cast<int>(perimeter / kMembraneSpacing));
    const float pointRadius = 0.6f * perimeter / static_cast<float>(n);

    const entt::entity e = registry_.create();
    const auto id = static_cast<std::uint32_t>(entt::to_integral(e));
    RingBody body;
    body.id = id;
    body.shapeRate = 0.0f;
    for (int i = 0; i < n; ++i) {
        const Vec2 local = contour(6.2831853f * static_cast<float>(i) / static_cast<float>(n));
        // Masa inversa 0: en los contactos cede siempre la célula, nunca el grano.
        body.ring.push_back(bodies_.allocate(BodyKind::Membrane, at.x + local.x, at.y + local.y, pointRadius, 0.0f, id));
        body.restRing.push_back(local);
    }
    for (int i = 0; i < n; ++i)
        body.restEdge.push_back(length(body.restRing[(i + 1) % n] - body.restRing[i]));
    body.restArea = polygon::signedArea(body.restRing.data(), n);
    soft_body::refresh(body, bodies_);
    soft_body::wrap(body, bodies_, width_, height_);
    soft_body::refresh(body, bodies_);
    registry_.emplace<RingBody>(e, std::move(body));
    registry_.emplace<Rock>(e, Rock{kind, static_cast<std::uint32_t>(rng.uniform() * 4294967040.0f)});
    rebuildFlowMask();
    return true;
}

entt::entity World::rockAt(Vec2 position) const {
    for (const auto [e, body, rock] : registry_.view<RingBody, Rock>().each()) {
        const Vec2 q = body.centroid + Vec2{minImage(position.x - body.centroid.x, width_),
                                            minImage(position.y - body.centroid.y, height_)};
        if (polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), q))
            return e;
    }
    return entt::null;
}

void World::rebuildFlowMask() {
    // El agua no fluye dentro de los granos (velocidad cero, como en el vidrio de la placa).
    // Simplificación: la química sí difunde a través de ellos (en la película el grano no
    // ocupa todo el espesor y extender las celdas fantasma a obstáculos arbitrarios es caro).
    const Grid2D& g = oxygen_.grid();
    const std::size_t n = g.v.size();
    if (dishMask_.flow.size() == n)
        flowMask_ = dishMask_.flow;
    else
        flowMask_.assign(n, 1.0f);
    bool any = dish_.active();
    for (const auto [e, body, rock] : registry_.view<RingBody, Rock>().each()) {
        any = true;
        const int pn = static_cast<int>(body.poly.size());
        const int x0 = static_cast<int>(std::floor(body.aabbMin.x / g.dx)), x1 = static_cast<int>(std::ceil(body.aabbMax.x / g.dx));
        const int y0 = static_cast<int>(std::floor(body.aabbMin.y / g.dx)), y1 = static_cast<int>(std::ceil(body.aabbMax.y / g.dx));
        for (int j = y0; j <= y1; ++j)
            for (int i = x0; i <= x1; ++i) {
                const Vec2 c = {(static_cast<float>(i) + 0.5f) * g.dx, (static_cast<float>(j) + 0.5f) * g.dx};
                if (polygon::contains(body.poly.data(), pn, c))
                    flowMask_[static_cast<std::size_t>(wrapIndex(j, g.ny)) * g.nx + wrapIndex(i, g.nx)] = 0.0f;
            }
    }
    fluid_.setFlowMask(any ? &flowMask_ : nullptr);
}

void World::scatterSediment() {
    // Sedimento del bioma: los granos primero (necesitan lugar libre), después el limo suelto.
    const Sediment& sd = config_.sediment;
    for (int k = 0, attempts = 0; k < sd.rocks && attempts < 50 * sd.rocks; ++attempts) {
        Rng rng(config_.seed, rng_stream::kSpawn + 80, static_cast<std::uint64_t>(attempts), 0);
        const float r = rng.uniform(sd.rockMin, sd.rockMax);
        // Granos de cuarzo; algunos con pátina de óxido de hierro.
        const MineralKind kind = rng.uniform() < 0.8f ? MineralKind::Quartz : MineralKind::IronOxide;
        const Vec2 at = randomPoint(rng, r * 1.3f + 4.0f);
        // Lejos del centro, donde nace el jugador (como la fauna silvestre).
        if (std::abs(minImage(at.x - 0.5f * width_, width_)) < 250.0f + r && std::abs(minImage(at.y - 0.5f * height_, height_)) < 200.0f + r)
            continue;
        k += spawnRock(at, r, kind, rng) ? 1 : 0;
    }
    const int counts[] = {sd.clay, sd.quartz, sd.mica, sd.ironOxide, sd.sulfur, sd.frustules};
    std::uint64_t idx = 0;
    for (int kind = 0; kind < static_cast<int>(MineralKind::Count); ++kind) {
        for (int k = 0; k < counts[kind]; ++k) {
            Rng rng(config_.seed, rng_stream::kSpawn + 81, idx++, 0);
            Vec2 at = randomPoint(rng);
            // La arcilla forma flóculos: grupos de partículas pegadas por la materia orgánica.
            if (kind == static_cast<int>(MineralKind::Clay) && k % 8 != 0 && ambient_.size() > 0 &&
                ambient_.kind.back() == ParticleKind::Mineral && ambient_.mineral.back() == MineralKind::Clay) {
                const float a = rng.uniform(0.0f, 2.0f * units::kPi), d = rng.uniform(1.0f, 6.0f);
                at = {ambient_.x.back() + d * std::cos(a), ambient_.y.back() + d * std::sin(a)};
            }
            spawnMineral(static_cast<MineralKind>(kind), at, rng);
        }
    }
}

void World::spawnDust(float dt) {
    // Polvo atmosférico que se deposita en la película: sobre todo limo de cuarzo y arcilla.
    dustAccumulator_ += medium_.dustRate * dt;
    while (dustAccumulator_ >= 1.0f) {
        dustAccumulator_ -= 1.0f;
        Rng rng(config_.seed, rng_stream::kSpawn + 82, spawnCounter_++, 0);
        const float u = rng.uniform();
        const MineralKind kind = u < 0.5f ? MineralKind::Clay : u < 0.9f ? MineralKind::Quartz : MineralKind::Mica;
        spawnMineral(kind, randomPoint(rng), rng);
    }
}
