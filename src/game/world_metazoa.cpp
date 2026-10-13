// Metazoos (fase 9): rotíferos como cuerpos compuestos con huevos partenogenéticos, Hydra con
// tentáculos y nematocistos, y nauplios de copépodo que llegan a la gota.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kTentacleNodes = 16;
constexpr float kTentacleContraction = 12.0f; // [µm/s] el tentáculo acerca la presa a la boca
constexpr float kMouthRadius = 35.0f;          // [µm] la presa entra cuando llega acá
constexpr float kEggFraction = 0.35f;          // huevo de Brachionus: ~1/3 de la madre

// Polo anterior (el punto de reposo con mayor x local) y dirección del cuerpo.
std::size_t frontIndex(const RingBody& body) {
    std::size_t front = 0;
    for (std::size_t i = 1; i < body.restRing.size(); ++i)
        if (body.restRing[i].x > body.restRing[front].x)
            front = i;
    return front;
}
} // namespace

void World::buildCompoundParts(entt::entity organism, const SpeciesInfo& info, const CellSpec& spec) {
    // Rotífero: corona (disco ciliado ancho) delante del tronco y pie con dedos detrás.
    const Vec2 dir = {std::cos(spec.angle), std::sin(spec.angle)};
    CellSpec head = spec;
    head.radiusX = 0.42f * spec.radiusX;
    head.radiusY = 0.85f * spec.radiusY;
    head.center = spec.center + dir * (spec.radiusX + 0.4f * head.radiusX); // se superponen: un solo animal
    head.shapeRate = 6.0f;
    head.r = static_cast<std::uint8_t>(std::min(255, spec.r + 20));
    head.g = static_cast<std::uint8_t>(std::min(255, spec.g + 20));
    head.b = static_cast<std::uint8_t>(std::min(255, spec.b + 20));
    CellSpec foot = spec;
    foot.radiusX = 0.55f * spec.radiusX;
    foot.radiusY = 0.22f * spec.radiusY;
    foot.center = spec.center - dir * (spec.radiusX + 0.5f * foot.radiusX);
    (void)info;
    const entt::entity h = spawnCell(head);
    const entt::entity f = spawnCell(foot);
    registry_.emplace<Segment>(h, Segment{organism});
    registry_.emplace<Segment>(f, Segment{organism});
    registry_.emplace<Compound>(organism, Compound{{h, f}});
    linkCells(h, organism, true);
    linkCells(organism, f, true);
}

void World::growTentacles(entt::entity e, const SpeciesInfo& info) {
    Tentacles t;
    for (int k = 0; k < info.tentacles; ++k) {
        Tentacle arm;
        // Corona de tentáculos alrededor del hipostoma (la boca), abiertos hacia adelante.
        arm.baseAngle = -1.3f + 2.6f * (static_cast<float>(k) + 0.5f) / static_cast<float>(info.tentacles);
        arm.phase = 1.7f * static_cast<float>(k);
        arm.nodes.assign(kTentacleNodes, Vec2{});
        t.arms.push_back(std::move(arm));
    }
    registry_.emplace<Tentacles>(e, std::move(t));
}

void World::destroyOrganism(entt::entity e) {
    std::vector<entt::entity> parts;
    if (const Compound* c = registry_.try_get<Compound>(e))
        parts = c->parts;
    for (entt::entity p : parts)
        if (registry_.valid(p) && registry_.all_of<RingBody>(p))
            destroyCell(registry_, bodies_, p);
    destroyCell(registry_, bodies_, e);
}

void World::updateTentacles(float dt) {
    PROFILE_SCOPE("World::updateTentacles");
    std::vector<std::pair<entt::entity, entt::entity>> toSwallow;
    for (auto [e, body, ten] : registry_.view<RingBody, Tentacles>().each()) {
        if (registry_.all_of<Cyst>(e))
            continue;
        const SpeciesInfo& info = speciesInfo(registry_.get<Species>(e).id);
        const std::size_t fi = frontIndex(body);
        const Vec2 mouth = {bodies_.x[body.ring[fi]], bodies_.y[body.ring[fi]]};
        Vec2 dir = mouth - body.centroid;
        dir = dir * (1.0f / std::max(1e-3f, length(dir)));
        const bool hungry = !isSatiated(e);
        for (Tentacle& arm : ten.arms) {
            // Presa sujeta: el tentáculo se acorta y la lleva a la boca.
            const bool holding = arm.prey != entt::null && registry_.valid(arm.prey) && registry_.all_of<Stung>(arm.prey);
            if (!holding)
                arm.prey = entt::null;
            const float maxLen = info.tentacleLength;
            if (holding)
                arm.extension -= kTentacleContraction / maxLen * dt;
            else
                arm.extension += 0.03f * dt; // se vuelve a extender despacio
            arm.extension = std::clamp(arm.extension, 0.05f, 1.0f);
            arm.phase += 0.6f * dt;
            // Cadena cinemática: se curva con una onda lenta (los tentáculos "barren" el agua).
            const float seg = maxLen * arm.extension / static_cast<float>(kTentacleNodes - 1);
            Vec2 p = mouth + rotate(dir, std::cos(arm.baseAngle), std::sin(arm.baseAngle)) * 10.0f;
            float a = 1.2f * arm.baseAngle;
            for (int j = 0; j < kTentacleNodes; ++j) {
                arm.nodes[static_cast<std::size_t>(j)] = p;
                const float t = static_cast<float>(j) / kTentacleNodes;
                a += 0.12f * std::sin(arm.phase + 0.5f * static_cast<float>(j)) * (0.3f + t);
                p = p + rotate(dir, std::cos(a), std::sin(a)) * seg;
            }
            if (holding) {
                const Vec2 hold = arm.nodes[static_cast<std::size_t>(arm.preyNode)];
                registry_.get<Stung>(arm.prey).point = wrapped(hold);
                if (length(hold - mouth) < kMouthRadius)
                    toSwallow.push_back({e, arm.prey});
                continue;
            }
            if (!hungry)
                continue;
            // Nematocistos: la presa que toca el tentáculo queda paralizada y pegada.
            Vec2 lo = arm.nodes[0], hi = arm.nodes[0];
            for (const Vec2& q : arm.nodes) {
                lo = {std::min(lo.x, q.x), std::min(lo.y, q.y)};
                hi = {std::max(hi.x, q.x), std::max(hi.y, q.y)};
            }
            for (const RingBody* other : bodyList_) {
                const auto oe = static_cast<entt::entity>(other->id);
                if (oe == e || !registry_.valid(oe))
                    continue;
                const Species* os = registry_.try_get<Species>(oe);
                if (!os || !(info.preyMask & speciesBit(os->id)) || registry_.any_of<Cyst, Stung, Captured>(oe))
                    continue;
                const Vec2 shift = imageShift(*other, (lo + hi) * 0.5f);
                if (other->aabbMax.x + shift.x < lo.x - 4.0f || other->aabbMin.x + shift.x > hi.x + 4.0f ||
                    other->aabbMax.y + shift.y < lo.y - 4.0f || other->aabbMin.y + shift.y > hi.y + 4.0f)
                    continue;
                const int pn = static_cast<int>(other->poly.size());
                for (int j = 2; j < kTentacleNodes; ++j) {
                    const Vec2 q = arm.nodes[static_cast<std::size_t>(j)] - shift;
                    if (polygon::contains(other->poly.data(), pn, q) ||
                        polygon::closestOnBoundary(other->poly.data(), pn, q).dist2 < 16.0f) {
                        registry_.emplace<Stung>(oe, Stung{e, wrapped(arm.nodes[static_cast<std::size_t>(j)])});
                        arm.prey = oe;
                        arm.preyNode = j;
                        ++observed_.hydraCaptures;
                        break;
                    }
                }
                if (arm.prey != entt::null)
                    break;
            }
        }
    }
    // Presas cuya Hydra murió: el veneno pasa y se sueltan.
    std::vector<entt::entity> freed;
    for (auto [pe, st] : registry_.view<Stung>().each())
        if (!registry_.valid(st.by))
            freed.push_back(pe);
    for (entt::entity pe : freed)
        registry_.remove<Stung>(pe);
    for (const auto& [hydra, prey] : toSwallow)
        if (registry_.valid(hydra) && registry_.valid(prey))
            swallow(hydra, prey);
    // Destruir presas compacta el almacenamiento de RingBody: los punteros de bodyList_ que usa
    // updateFeeding quedarían colgados.
    if (!toSwallow.empty())
        collectBodies();
}

void World::layEgg(entt::entity mother) {
    // Brachionus: la hembra amíctica pone un huevo diploide (partenogénesis) que lleva pegado
    // a la lórica, junto al pie, hasta que eclosiona.
    const SpeciesId sid = registry_.get<Species>(mother).id;
    const SpeciesInfo& info = speciesInfo(sid);
    Metabolism& m = registry_.get<Metabolism>(mother);
    const float eggBiomass = kEggFraction * m.birthBiomass;
    m.biomass -= eggBiomass;
    const RingBody& body = registry_.get<RingBody>(mother);
    const Vec2 c = body.centroid;
    const std::size_t fi = frontIndex(body);
    Vec2 dir = Vec2{bodies_.x[body.ring[fi]], bodies_.y[body.ring[fi]]} - c;
    dir = dir * (1.0f / std::max(1e-3f, length(dir)));
    const float half = 0.5f * std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y);
    const Lineage lin = registry_.all_of<Lineage>(mother) ? registry_.get<Lineage>(mother) : Lineage{0, 0};
    Genome genome = registry_.all_of<Heredity>(mother) ? registry_.get<Heredity>(mother).genome : speciesGenome(sid);
    Rng rng(config_.seed, rng_stream::kMutation + 70, body.id, step_);
    mutate(genome, info.locomotion, rng);
    const std::uint32_t bodyId = body.id;

    CellSpec egg;
    const float r = std::sqrt(cellAreaForMass(eggBiomass) / units::kPi);
    egg.radiusX = 1.2f * r;
    egg.radiusY = 0.85f * r;
    egg.angle = std::atan2(dir.y, dir.x);
    egg.center = c - dir * (0.6f * half) + Vec2{-dir.y, dir.x} * (0.7f * half + egg.radiusY);
    egg.shapeRate = 10.0f; // cáscara
    egg.r = 214;
    egg.g = 210;
    egg.b = 186;
    const entt::entity e = spawnCell(egg);
    registry_.emplace<Egg>(e, Egg{genome, sid, info.eggHours, eggBiomass, lin.run, lin.generation + 1});
    linkCells(mother, e);
    ++observed_.eggsLaid;
    (void)bodyId;
}

void World::updateEggs(float bioDt) {
    std::vector<entt::entity> hatching;
    for (auto [e, egg] : registry_.view<Egg>().each()) {
        egg.hoursLeft -= bioDt / 3600.0f;
        if (egg.hoursLeft <= 0.0f)
            hatching.push_back(e);
    }
    for (entt::entity e : hatching) {
        const Egg egg = registry_.get<Egg>(e);
        const RingBody& b = registry_.get<RingBody>(e);
        const Vec2 at = b.centroid;
        const float angle = b.angle;
        destroyCell(registry_, bodies_, e);
        // La cría eclosiona casi del tamaño adulto (eutelia: no suma células, solo crece).
        const entt::entity young = spawnOrganism(egg.species, at, egg.genome, egg.run, angle);
        Metabolism& m = registry_.get<Metabolism>(young);
        m.birthBiomass = m.biomass = std::min(m.biomass, egg.biomass * 2.0f);
        m.reserves = 0.3f * m.biomass;
        registry_.get<Lineage>(young).generation = egg.generation;
        registry_.emplace<Autopilot>(young);
        ++observed_.eggsHatched;
    }
}

void World::arrivals(float dt) {
    // Nauplios: eclosionan de los sacos de huevos de copépodos adultos (demasiado grandes para
    // esta gota) que pasan por el agua de alrededor.
    arrivalClock_ += medium_.naupliusArrivalsPerHour * dt * bioScale() / 3600.0f;
    if (arrivalClock_ < 1.0f)
        return;
    arrivalClock_ -= 1.0f;
    int present = 0;
    for (auto [e, sp] : registry_.view<Species>().each())
        present += sp.id == SpeciesId::Nauplius ? 1 : 0;
    if (present >= 3)
        return;
    Rng rng(config_.seed, rng_stream::kSpawn + 50, spawnCounter_++, 0);
    const Vec2 at = {rng.uniform(0.0f, width_), rng.uniform(0.0f, height_)};
    const entt::entity n = spawnOrganism(SpeciesId::Nauplius, at, speciesGenome(SpeciesId::Nauplius), 0,
                                         rng.uniform(0.0f, 2.0f * units::kPi));
    registry_.emplace<Autopilot>(n);
    ++observed_.arrivals;
}
