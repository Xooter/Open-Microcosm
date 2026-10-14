// Hongos en el mundo: zoosporas que buscan hospedador o sustrato, quitridios sobre algas,
// micelios que descomponen detritos.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Algas que parasitan los quitridios (Rhizophydium spp. infectan diatomeas y clorofitas; hay
// quitridios específicos de Oedogonium y de las zignematales).
constexpr std::uint32_t kChytridHosts =
    speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Navicula) | speciesBit(SpeciesId::Pediastrum) |
    speciesBit(SpeciesId::Mougeotia) | speciesBit(SpeciesId::Zygnema) | speciesBit(SpeciesId::Oedogonium);

// Fotobiontes de los líquenes: algas verdes (los líquenes reales usan Trebouxia y afines).
constexpr std::uint32_t kPhotobionts = speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Pediastrum);

bool hostOf(const entt::registry& reg, entt::entity e, std::uint32_t mask) {
    const Species* sp = reg.try_get<Species>(e);
    return sp && (mask & speciesBit(sp->id));
}

} // namespace

void World::seedFungi() {
    // Al empezar: algunos micelios de Saprolegnia sobre detritos y zoosporas de quitridios en el
    // agua.
    auto spawn = [&](FungusKind kind, std::uint64_t salt) {
        Rng rng(config_.seed, rng_stream::kFungi + 400, salt, 0);
        Vec2 at = randomPoint(rng, 20.0f);
        if (ambient_.size() > 0 && kind == FungusKind::Saprolegnia) {
            const auto i = static_cast<std::size_t>(rng.uniform() * static_cast<float>(ambient_.size())) % ambient_.size();
            if (ambient_.kind[i] == ParticleKind::Detritus) // germina sobre materia orgánica, no sobre minerales
                at = {ambient_.x[i], ambient_.y[i]};
        }
        spawnFungus(kind, at, rng.uniform(0.0f, 2.0f * units::kPi), salt);
    };
    for (std::uint64_t k = 0; k < static_cast<std::uint64_t>(config_.saprolegnia); ++k)
        spawn(FungusKind::Saprolegnia, k);
    for (std::uint64_t k = 0; k < static_cast<std::uint64_t>(config_.allomyces); ++k)
        spawn(FungusKind::Allomyces, 200 + k);
    // Líquenes: el micobionte nace junto a un alga verde (si hay) para encontrar su fotobionte.
    for (std::uint64_t k = 0; k < static_cast<std::uint64_t>(config_.lichens); ++k) {
        Rng rng(config_.seed, rng_stream::kFungi + 400, 300 + k, 0);
        Vec2 at = randomPoint(rng, 20.0f);
        std::size_t seen = 0;
        for (auto [e, body] : registry_.view<RingBody>().each())
            if (hostOf(registry_, e, kPhotobionts) && rng.uniform() * static_cast<float>(++seen) < 1.0f)
                at = dish_.clampInside(body.centroid + Vec2{20.0f, 0.0f}, 20.0f);
        spawnFungus(FungusKind::Mycobiont, at, rng.uniform(0.0f, 2.0f * units::kPi), 300 + k);
    }
    seedPlasmodia();
    for (std::uint64_t k = 0; k < static_cast<std::uint64_t>(config_.chytridClusters); ++k) {
        Rng rng(config_.seed, rng_stream::kFungi + 500, k, 0);
        spawnFungus(FungusKind::Chytrid, randomPoint(rng, 10.0f), 0.0f, 9000 + k);
    }
}

void World::spawnFungus(FungusKind kind, Vec2 at, float heading, std::uint64_t salt) {
    // El fósforo sale del agua (se conserva).
    const FungiParams& p = fungi_.params;
    if (kind == FungusKind::Chytrid) {
        const int count = 10;
        const float phosphorus = -exchange(phosphate_, at, -static_cast<float>(count) * p.sporePmol * p.phosphorusRatio, Flow::Spawn);
        fungi_.spawnSpores(FungusKind::Chytrid, at, count, phosphorus, config_.seed, salt);
        return;
    }
    const float reserves = 20.0f * p.nodePmol;
    const float phosphorus = -exchange(phosphate_, at, -2.0f * p.phosphorusRatio * reserves, Flow::Spawn);
    fungi_.spawnMycelium(kind, at, heading, reserves, phosphorus);
}

void World::stepFungi(float dt) {
    PROFILE_SCOPE("World::stepFungi");
    const float bioDt = dt * bioScale();
    const FungiParams& p = fungi_.params;
    Zoospores& sp = fungi_.spores;

    // Quimiotaxis: los quitridios hacia el alga hospedadora más cercana; Saprolegnia sube el
    // gradiente de materia orgánica disuelta (aminoácidos).
    sporeSteer_.assign(sp.size(), Vec2{});
    hostCells_.clear();
    photoCells_.clear();
    photoPoints_.clear();
    for (auto [e, body] : registry_.view<RingBody>().each()) {
        // Los líquenes protegen a su alga con metabolitos secundarios: no la infectan.
        if (hostOf(registry_, e, kChytridHosts) && !registry_.any_of<Cyst, Lichenized>(e))
            hostCells_.push_back(e);
        if (hostOf(registry_, e, kPhotobionts) && !registry_.any_of<Cyst, Lichenized>(e)) {
            photoCells_.push_back(e);
            photoPoints_.push_back(body.centroid);
        }
    }
    const Grid2D& food = nutrients_.grid();
    for (std::size_t i = 0; i < sp.size(); ++i) {
        const Vec2 at = {sp.x[i], sp.y[i]};
        const SporeSense sense = fungusInfo(sp.kind[i]).sense;
        if (sense == SporeSense::Host) {
            float best = p.sporeSenseRange * p.sporeSenseRange;
            for (entt::entity h : hostCells_) {
                const RingBody& b = registry_.get<RingBody>(h);
                const Vec2 d = {minImage(b.centroid.x - at.x, width_), minImage(b.centroid.y - at.y, height_)};
                if (dot(d, d) < best) {
                    best = dot(d, d);
                    sporeSteer_[i] = d;
                }
            }
        } else if (sense == SporeSense::Food) {
            const float g = food.dx;
            sporeSteer_[i] = {food.sample(at.x + g, at.y) - food.sample(at.x - g, at.y),
                              food.sample(at.x, at.y + g) - food.sample(at.x, at.y - g)};
        }
    }
    fungi_.moveSpores(fluid_, sporeSteer_, dt, config_.seed, step_, width_, height_);

    // Encuentro: la zoospora que toca un alga se enquista sobre su pared.
    for (std::size_t i = sp.size(); i-- > 0;) {
        if (sp.kind[i] != FungusKind::Chytrid)
            continue;
        const Vec2 at = {sp.x[i], sp.y[i]};
        for (entt::entity h : hostCells_) {
            const RingBody& b = registry_.get<RingBody>(h);
            const Vec2 q = b.centroid + Vec2{minImage(at.x - b.centroid.x, width_), minImage(at.y - b.centroid.y, height_)};
            if (q.x < b.aabbMin.x - 2.0f || q.x > b.aabbMax.x + 2.0f || q.y < b.aabbMin.y - 2.0f || q.y > b.aabbMax.y + 2.0f)
                continue;
            if (!polygon::contains(b.poly.data(), static_cast<int>(b.poly.size()), q) &&
                polygon::closestOnBoundary(b.poly.data(), static_cast<int>(b.poly.size()), q).dist2 > 4.0f)
                continue;
            ChytridInfection& inf = registry_.get_or_emplace<ChytridInfection>(h);
            if (static_cast<int>(inf.siteAngle.size()) >= p.chytridMaxSporangia)
                break;
            const Vec2 local = soft_body::toLocal(b, q);
            inf.siteAngle.push_back(std::atan2(local.y, local.x));
            inf.mass += p.sporePmol;
            inf.phosphorus += sp.phosphorus[i];
            sp.swapRemove(i);
            ++observed_.chytridInfections;
            break;
        }
    }

    // Micelios: absorción, crecimiento, esporas y germinación.
    FungiEnv env;
    env.food = [&](Vec2 at) { return nutrients_.grid().sample(at.x, at.y); };
    env.takeFood = [&](Vec2 at, float pmol) { return -exchange(nutrients_, wrapped(at), -pmol, Flow::Uptake); };
    env.takePhosphate = [&](Vec2 at, float pmol) { return -exchange(phosphate_, wrapped(at), -pmol, Flow::Uptake); };
    env.release = [&](Vec2 at, float pmol, float phosphorus) {
        if (pmol > 0.0f)
            exchange(nutrients_, wrapped(at), pmol, Flow::Death);
        if (phosphorus > 0.0f)
            exchange(phosphate_, wrapped(at), phosphorus, Flow::Death);
    };
    env.photobiont = [&](Vec2 at, float range, Vec2& dir) {
        float best = range * range;
        bool found = false;
        for (const Vec2& c : photoPoints_) {
            const Vec2 d = {minImage(c.x - at.x, width_), minImage(c.y - at.y, height_)};
            if (dot(d, d) < best) {
                best = dot(d, d);
                dir = d;
                found = true;
            }
        }
        return found;
    };
    env.substrate = [&](Vec2 at) {
        const Vec2 w = wrapped(at);
        return shading_.sample(w.x, w.y) > 0.02f || nutrients_.grid().sample(w.x, w.y) > 2.0f;
    };
    fungi_.grow(env, bioDt, config_.seed, step_);
    observed_.restingFormed = fungi_.stats.restingFormed;
    observed_.restingGerminated = fungi_.stats.restingGerminated;
    stepLichens(bioDt);

    // Cobertura de hifas (para las exoenzimas que disuelven detritos).
    std::fill(hyphaCover_.v.begin(), hyphaCover_.v.end(), 0.0f);
    for (const Mycelium& m : fungi_.mycelia)
        for (std::size_t k = 0; k < m.x.size(); ++k) {
            const int cx = hyphaCover_.cellX(wrapCoord(m.x[k], width_)), cy = hyphaCover_.cellY(wrapCoord(m.y[k], height_));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    hyphaCover_.at(wrapIndex(cx + dx, hyphaCover_.nx), wrapIndex(cy + dy, hyphaCover_.ny)) = 1.0f;
        }

    updateInfections(bioDt);
}

void World::updateInfections(float bioDt) {
    const FungiParams& p = fungi_.params;
    const float hours = bioDt / 3600.0f;
    // Quitridios: los rizoides absorben el citoplasma del alga; al madurar, liberan zoosporas.
    for (auto [e, inf, meta] : registry_.view<ChytridInfection, Metabolism>().each()) {
        inf.ageHours += hours;
        const float n = static_cast<float>(inf.siteAngle.size());
        const float f = 1.0f - std::exp(-p.chytridDrainPerHour * n * hours);
        const float fromReserves = meta.reserves * f, fromBiomass = meta.biomass * f;
        meta.reserves -= fromReserves;
        meta.biomass -= fromBiomass;
        inf.mass += p.chytridYield * (fromReserves + fromBiomass);
        const float pOut = meta.phosphorus * f;
        meta.phosphorus -= pOut;
        inf.phosphorus += pOut;
        if (inf.ageHours >= p.chytridMaxHours)
            releaseChytridSpores(e);
    }
}

void World::releaseChytridSpores(entt::entity host) {
    ChytridInfection* inf = registry_.try_get<ChytridInfection>(host);
    if (!inf)
        return;
    const FungiParams& p = fungi_.params;
    const RingBody& body = registry_.get<RingBody>(host);
    const int maxSpores = p.chytridMaxSpores * static_cast<int>(inf->siteAngle.size());
    int count = std::min(maxSpores, static_cast<int>(inf->mass / p.sporePmol));
    if (fungi_.spores.size() + static_cast<std::size_t>(std::max(count, 0)) > p.maxSpores)
        count = 0;
    if (count > 0) {
        fungi_.spawnSpores(FungusKind::Chytrid, body.centroid, count, inf->phosphorus, config_.seed, step_ * 4096ull + body.id);
        ++observed_.chytridReleases;
    } else if (registry_.all_of<Metabolism>(host)) {
        registry_.get<Metabolism>(host).phosphorus += inf->phosphorus; // nada maduró: vuelve al alga
    } else {
        exchange(phosphate_, body.centroid, inf->phosphorus, Flow::Death);
    }
    registry_.remove<ChytridInfection>(host);
}

Vec2 World::wrapped(Vec2 at) const { return {wrapCoord(at.x, width_), wrapCoord(at.y, height_)}; }

void World::stepLichens(float bioDt) {
    PROFILE_SCOPE("World::stepLichens");
    const FungiParams& p = fungi_.params;
    const float hours = bioDt / 3600.0f;
    // Encuentro: una hifa que toca un alga verde la sujeta con un haustorio (el contacto es la
    // simbiosis; la lógica del tropismo está en Fungi::grow).
    for (Mycelium& m : fungi_.mycelia) {
        if (!fungusInfo(m.kind).photobiont)
            continue;
        int linked = 0;
        for (auto [e, li] : registry_.view<Lichenized>().each())
            linked += li.mycelium == m.id;
        for (std::size_t k = 0; k < m.x.size() && linked < p.lichenMaxAlgae; ++k) {
            if (!m.tip[k])
                continue;
            const Vec2 at = wrapped({m.x[k], m.y[k]});
            for (entt::entity e : photoCells_) {
                if (!registry_.valid(e) || registry_.all_of<Lichenized>(e))
                    continue;
                const RingBody& b = registry_.get<RingBody>(e);
                const Vec2 q = b.centroid + Vec2{minImage(at.x - b.centroid.x, width_), minImage(at.y - b.centroid.y, height_)};
                if (q.x < b.aabbMin.x - 4.0f || q.x > b.aabbMax.x + 4.0f || q.y < b.aabbMin.y - 4.0f || q.y > b.aabbMax.y + 4.0f)
                    continue;
                if (polygon::closestOnBoundary(b.poly.data(), static_cast<int>(b.poly.size()), q).dist2 > 16.0f &&
                    !polygon::contains(b.poly.data(), static_cast<int>(b.poly.size()), q))
                    continue;
                registry_.emplace<Lichenized>(e, Lichenized{m.id, at, 0.0f});
                ++observed_.lichenLinks;
                ++linked;
                m.tip[k] = 0; // el haustorio detiene la punta
                break;
            }
        }
    }
    // Intercambio: el alga cede azúcar (polioles); el hongo le devuelve fósforo de su sobrante.
    for (auto [e, li, meta] : registry_.view<Lichenized, Metabolism>().each()) {
        Mycelium* m = fungi_.find(li.mycelium);
        if (!m) {
            registry_.remove<Lichenized>(e); // el micelio murió: el alga queda libre
            continue;
        }
        const float sugar = meta.reserves * (1.0f - std::exp(-p.lichenSugarPerHour * hours));
        meta.reserves -= sugar;
        li.transferred += sugar;
        fungi_.feed(m->id, sugar, 0.0f);
        const float need = p.phosphorusRatio * static_cast<float>(m->x.size()) * p.nodePmol;
        const float surplus = std::max(0.0f, m->phosphorus - 1.5f * need);
        const float toAlga = surplus * (1.0f - std::exp(-p.lichenPhosphorusPerHour * hours));
        m->phosphorus -= toAlga;
        meta.phosphorus += toAlga;
    }
}
