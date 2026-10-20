// Semicélula nueva de las desmidias (Cosmarium, Micrasterias): la célula se parte por el istmo, cada
// hija se queda con una semicélula vieja y la otra brota del istmo como una yema redonda que se expande y
// toma su forma (en Micrasterias, los lóbulos) en unas horas (Meindl 1993, Microbiol. Rev. 57: 415).
// Simplificación: la semicélula vieja queda a la escala de la hija (en la realidad conserva la de la
// madre); así la masa y el área siguen la misma relación que en el resto de la placa.
#include "core/profiling.hpp"
#include "game/rest_shape.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Tamaño relativo de la yema recién formada.
constexpr float kBud = 0.15f;

void applySemicell(RingBody& body, SemicellGrowth& g) {
    const float t = std::clamp(g.progress, 0.0f, 1.0f);
    const float size = kBud + (1.0f - kBud) * t * t * (3.0f - 2.0f * t);
    thread_local std::vector<Vec2> shape;
    shape = g.full;
    for (Vec2& q : shape)
        if (q.x * g.newSign > 0.0f) { // la mitad nueva crece desde el istmo
            q.x *= size;
            q.y = std::copysign(g.isthmus + (std::abs(q.y) - g.isthmus) * size, q.y);
        }
    const int n = static_cast<int>(shape.size());
    const float a = std::abs(polygon::signedArea(shape.data(), n));
    const float a0 = std::abs(polygon::signedArea(g.full.data(), n));
    const float scale = a0 > 0.0f ? a / a0 : 1.0f;
    rest_shape::apply(body, shape, scale, g.areaScale);
    g.areaScale = scale;
}
} // namespace

void World::startSemicell(entt::entity e, entt::entity sibling) {
    if (!registry_.all_of<RingBody>(e) || !registry_.all_of<RingBody>(sibling))
        return;
    RingBody& body = registry_.get<RingBody>(e);
    const RingBody& other = registry_.get<RingBody>(sibling);
    SemicellGrowth g;
    g.full = body.restRing;
    // La semicélula nueva es la del lado de la hermana (las dos se separan por el istmo).
    const Vec2 d = other.centroid + imageShift(other, body.centroid) - body.centroid;
    const float co = std::cos(body.angle), si = std::sin(body.angle);
    g.newSign = d.x * co + d.y * si >= 0.0f ? 1.0f : -1.0f;
    // Medio ancho del istmo: el fondo de los senos, junto a x = 0.
    float rx = 0.0f;
    for (const Vec2& q : g.full)
        rx = std::max(rx, std::abs(q.x));
    float sum = 0.0f;
    int count = 0;
    for (const Vec2& q : g.full)
        if (std::abs(q.x) < 0.08f * rx) {
            sum += std::abs(q.y);
            ++count;
        }
    g.isthmus = count > 0 ? sum / static_cast<float>(count) : 0.3f * rx;
    SemicellGrowth& placed = registry_.emplace_or_replace<SemicellGrowth>(e, std::move(g));
    applySemicell(body, placed);
}

void World::updateSemicells(float dt) {
    PROFILE_SCOPE("World::updateSemicells");
    const float hours = dt * bioScale() / 3600.0f;
    thread_local std::vector<entt::entity> done;
    done.clear();
    for (auto [e, g, body] : registry_.view<SemicellGrowth, RingBody>().each()) {
        if (registry_.any_of<Dividing, Cyst>(e) || g.full.size() != body.restRing.size()) {
            done.push_back(e);
            continue;
        }
        const float hoursTotal = speciesOfCell(e).semicellRegrowthHours;
        g.progress = hoursTotal > 0.0f ? g.progress + hours / hoursTotal : 1.0f;
        applySemicell(body, g);
        if (g.progress >= 1.0f)
            done.push_back(e);
    }
    for (entt::entity e : done)
        registry_.remove<SemicellGrowth>(e);
}

void World::finishSemicell(entt::entity e, RingBody& body) {
    // Antes de dividirse otra vez, la semicélula nueva ya está completa.
    SemicellGrowth* g = registry_.try_get<SemicellGrowth>(e);
    if (!g)
        return;
    if (g->full.size() == body.restRing.size()) {
        g->progress = 1.0f;
        applySemicell(body, *g);
    }
    registry_.remove<SemicellGrowth>(e);
}

float World::restShapeAreaScale(entt::entity e) const {
    const SemicellGrowth* g = registry_.try_get<SemicellGrowth>(e);
    return contractionAreaScale(e) * (g ? g->areaScale : 1.0f);
}
