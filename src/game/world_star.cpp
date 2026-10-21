// Colonia en estrella de Asterionella formosa: células en aguja que irradian de un centro, unidas por el
// polo posterior (el "pie") con almohadillas de mucílago, a ángulos casi iguales; ~8 por colonia (Kreutz;
// Round, Crawford & Mann 1990, The Diatoms). La colonia crece por división: la hija queda unida a la
// madre y la estrella se reacomoda.
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Las almohadillas de mucílago dejan los pies a unos µm del centro de la estrella.
constexpr float kPad = 4.0f; // [µm]
// Sin inercia: cada miembro se lleva a su lugar a esta tasa (como las colonias deslizantes).
constexpr float kRate = 2.0f; // [1/s]

Vec2 rotated(Vec2 v, float a) { return {v.x * std::cos(a) - v.y * std::sin(a), v.x * std::sin(a) + v.y * std::cos(a)}; }
} // namespace

void World::foundStarColony(entt::entity root, Rng& rng, bool randomStage) {
    const SpeciesInfo& info = speciesOfCell(root);
    const RingBody& rb = registry_.get<RingBody>(root);
    const SpeciesId id = registry_.get<Species>(root).id;
    float half = 0.0f;
    for (const Vec2& q : rb.restRing)
        half = std::max(half, std::abs(q.x));
    // Colonias de la mitad al máximo de células (las más chicas se están armando).
    const int cells = std::clamp(static_cast<int>(rng.uniform(0.5f, 1.0f) * static_cast<float>(info.starColonyMax) + 0.5f), 2,
                                 info.starColonyMax);
    const Vec2 h = bodyHeading(rb);
    const Vec2 hub = rb.centroid - h * (half + kPad);
    registry_.emplace_or_replace<StarMember>(root, StarMember{root, 0});
    for (int k = 1; k < cells; ++k) {
        const float a = 2.0f * units::kPi * static_cast<float>(k) / static_cast<float>(info.starColonyMax);
        const Vec2 d = rotated(h, a);
        const float angle = std::atan2(d.y, d.x);
        // La colonia es un clon de la célula que la fundó.
        const Genome founder = registry_.all_of<Heredity>(root) ? registry_.get<Heredity>(root).genome : speciesGenome(id);
        const entt::entity e =
            spawnWildCell(id, dish_.clampInside(hub + d * (half + kPad), half), rng, randomStage, &angle, 0.0f, &founder);
        registry_.emplace_or_replace<StarMember>(e, StarMember{root, k});
    }
}

bool World::joinStarColony(entt::entity mother, entt::entity child) {
    const SpeciesInfo& info = speciesOfCell(mother);
    StarMember* mm = registry_.try_get<StarMember>(mother);
    const entt::entity root = mm && registry_.valid(mm->root) ? mm->root : mother;
    if (!mm)
        registry_.emplace<StarMember>(mother, StarMember{mother, 0});
    // Lugares ocupados de la colonia.
    std::vector<bool> used(static_cast<std::size_t>(info.starColonyMax), false);
    for (auto [e, m] : registry_.view<StarMember>().each())
        if (m.root == root && m.slot >= 0 && m.slot < info.starColonyMax)
            used[static_cast<std::size_t>(m.slot)] = true;
    for (int k = 1; k < info.starColonyMax; ++k)
        if (!used[static_cast<std::size_t>(k)]) {
            registry_.emplace_or_replace<StarMember>(child, StarMember{root, k});
            return true;
        }
    return false; // colonia completa: la hija se va sola
}

void World::applyStarColonies(float dt) {
    PROFILE_SCOPE("World::applyStarColonies");
    (void)dt;
    thread_local std::vector<entt::entity> broken;
    broken.clear();
    for (auto [e, m] : registry_.view<StarMember>().each()) {
        if (m.root == e)
            continue;
        // Sin la fundadora la colonia se deshace (las almohadillas se sueltan).
        if (!registry_.valid(m.root) || !registry_.all_of<RingBody, StarMember>(m.root)) {
            broken.push_back(e);
            continue;
        }
        if (registry_.any_of<Dividing>(e) || registry_.any_of<Dividing>(m.root) || !registry_.all_of<RingBody>(e))
            continue;
        RingBody& root = registry_.get<RingBody>(m.root);
        RingBody& b = registry_.get<RingBody>(e);
        const SpeciesInfo& info = speciesOfCell(m.root);
        const Vec2 hr = bodyHeading(root);
        const std::uint32_t rp = root.ring[poleIndex(root, false)];
        const Vec2 hub = Vec2{bodies_.x[rp], bodies_.y[rp]} - hr * kPad;
        const Vec2 d = rotated(hr, 2.0f * units::kPi * static_cast<float>(m.slot) / static_cast<float>(info.starColonyMax));
        // El pie del miembro va a su lugar junto al centro y su eje apunta hacia afuera.
        const std::uint32_t bp = b.ring[poleIndex(b, false)];
        const Vec2 foot = Vec2{bodies_.x[bp], bodies_.y[bp]};
        const Vec2 v = (hub + d * kPad - foot) * kRate;
        const Vec2 hb = bodyHeading(b);
        const float turn = std::atan2(cross(hb, d), dot(hb, d)) * kRate;
        for (std::uint32_t s : b.ring) {
            const Vec2 r = Vec2{bodies_.x[s], bodies_.y[s]} - foot; // gira alrededor del pie
            bodies_.vx[s] += v.x - r.y * turn;
            bodies_.vy[s] += v.y + r.x * turn;
        }
        // Reacción en la fundadora (la estrella entera queda sin fuerza neta).
        const float share = 1.0f / static_cast<float>(std::max(1, info.starColonyMax - 1));
        for (std::uint32_t s : root.ring) {
            bodies_.vx[s] -= v.x * share;
            bodies_.vy[s] -= v.y * share;
        }
    }
    for (entt::entity e : broken)
        registry_.remove<StarMember>(e);
}
