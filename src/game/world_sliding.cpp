// Colonia deslizante de Bacillaria paxillifer (docs/mechanics/stalks-and-colonies.md): células en aguja
// unidas lado a lado por el rafe, que se deslizan una sobre otra con la misma oscilación.
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

void World::joinSlidingColony(entt::entity mother, entt::entity child) {
    const SpeciesInfo& info = speciesOfCell(mother);
    // Tamaño de la colonia: las células unidas (por uniones) con la madre.
    std::vector<entt::entity> members{mother};
    for (std::size_t k = 0; k < members.size(); ++k)
        for (const SlidingJunction& j : slides_) {
            const entt::entity other = j.a == members[k] ? j.b : j.b == members[k] ? j.a : entt::null;
            if (other != entt::null && std::find(members.begin(), members.end(), other) == members.end())
                members.push_back(other);
        }
    if (static_cast<int>(members.size()) >= info.slidingColonyMax)
        return; // colonia completa: la hija se va sola
    // Del lado que la madre todavía no tiene ocupado.
    float side = 1.0f;
    for (const SlidingJunction& j : slides_)
        if (j.a == mother && j.side > 0.0f)
            side = -1.0f;
    slides_.push_back({mother, child, side});
}

void World::applySlidingColonies(float dt) {
    PROFILE_SCOPE("World::applySlidingColonies");
    (void)dt;
    std::erase_if(slides_, [&](const SlidingJunction& j) {
        return !registry_.valid(j.a) || !registry_.valid(j.b) || !registry_.all_of<RingBody>(j.a) ||
               !registry_.all_of<RingBody>(j.b) || registry_.any_of<Dividing>(j.a) || registry_.any_of<Dividing>(j.b);
    });
    for (const SlidingJunction& j : slides_) {
        RingBody& a = registry_.get<RingBody>(j.a);
        RingBody& b = registry_.get<RingBody>(j.b);
        const SpeciesInfo& info = speciesOfCell(j.a);
        const Vec2 h = bodyHeading(a);
        const Vec2 side = Vec2{-h.y, h.x} * j.side;
        float halfLength = 0.0f, halfA = 0.0f, halfB = 0.0f;
        for (const Vec2& q : a.restRing) {
            halfLength = std::max(halfLength, std::abs(q.x));
            halfA = std::max(halfA, std::abs(q.y));
        }
        for (const Vec2& q : b.restRing)
            halfB = std::max(halfB, std::abs(q.y));
        // Todos los pares con la misma fase: la colonia se estira en escalera y vuelve.
        const float offset = info.slidingAmplitude * 2.0f * halfLength *
                             std::sin(6.2831853f * simTime_ / info.slidingPeriod);
        const Vec2 bc = b.centroid + imageShift(b, a.centroid);
        const Vec2 target = a.centroid + side * (halfA + halfB + 1.0f) + h * offset;
        // Sin inercia: cada una se mueve hacia donde la lleva la unión (mitad y mitad), y b se alinea con a.
        constexpr float kRate = 2.0f; // [1/s]
        const Vec2 v = (target - bc) * (0.5f * kRate);
        const Vec2 hb = bodyHeading(b);
        const float sign = dot(hb, h) >= 0.0f ? 1.0f : -1.0f; // eje sin sentido
        const float turn = std::atan2(cross(hb * sign, h), dot(hb * sign, h)) * kRate;
        for (std::uint32_t s : b.ring) {
            const Vec2 r = Vec2{bodies_.x[s], bodies_.y[s]} - b.centroid;
            bodies_.vx[s] += v.x - r.y * turn;
            bodies_.vy[s] += v.y + r.x * turn;
        }
        for (std::uint32_t s : a.ring) {
            bodies_.vx[s] -= v.x;
            bodies_.vy[s] -= v.y;
        }
    }
}
