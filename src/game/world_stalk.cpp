// Pie y tallo de los seres fijos (mecanismo M9 de docs/plan_mecanicas.md): la varilla de bio/stalk con
// el agua de la placa, el zooide que sostiene, su crecimiento, el espasmonema y el tallo vacío.
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
const SpeciesInfo& infoOfEntity(const entt::registry& reg, entt::entity e) {
    static const SpeciesInfo kNone;
    const Species* sp = reg.try_get<Species>(e);
    return sp ? speciesInfo(sp->id) : kNone;
}
} // namespace

bool World::branchColony(entt::entity mother, entt::entity child) {
    const Anchored* an = registry_.try_get<Anchored>(mother);
    if (!an || !registry_.valid(an->rod) || !registry_.all_of<RingBody>(child))
        return false;
    Stalk* parent = registry_.try_get<Stalk>(an->rod);
    if (!parent)
        return false;
    const StalkSpec& spec = speciesInfo(parent->species).stalk;
    if (!spec.colony || parent->rod.nodes.size() < 2)
        return false;
    const entt::entity root = registry_.valid(parent->root) ? parent->root : an->rod;
    int zooids = 0;
    for (auto [se, st] : registry_.view<Stalk>().each())
        zooids += (se == root || st.root == root) && st.zooid != entt::null ? 1 : 0;
    if (zooids >= spec.maxZooids)
        return false; // colonia completa: la hija se va como telotroco
    // La rama sale de la punta del tallo de la madre, girada hacia un lado (alternando).
    const Vec2 tip = parent->rod.nodes.back(), prev = parent->rod.nodes[parent->rod.nodes.size() - 2];
    Vec2 dir = tip - prev;
    const float len = length(dir);
    dir = len > 1e-3f ? dir * (1.0f / len) : parent->rod.baseDirection;
    const float a = (parent->children % 2 == 0 ? 1.0f : -1.0f) * spec.branchAngle; // alternan de lado
    dir = {dir.x * std::cos(a) - dir.y * std::sin(a), dir.x * std::sin(a) + dir.y * std::cos(a)};
    const SpeciesId species = parent->species;
    const std::uint32_t id = nextStalkId_++;
    const entt::entity parentEntity = an->rod;
    const int depth = parent->depth + 1;
    ++parent->children;
    const entt::entity rod = registry_.create(); // ojo: puede realocar el almacenamiento de Stalk
    Stalk& st = registry_.emplace<Stalk>(rod);
    layStalk(st.rod, tip, dir, std::max(1.0f, 0.05f * spec.length), stalkNodeCount(spec.length));
    st.species = species;
    st.zooid = child;
    st.id = id;
    st.parent = parentEntity;
    st.root = root;
    st.depth = depth;
    st.turn = a;
    registry_.emplace_or_replace<Anchored>(child, Anchored{tip, st.rod.length, 0.0f, rod});
    return true;
}

void World::contractStalk(Anchored& anchor, float dt) {
    // Espasmonema: el Ca²⁺ dispara la espasmina y el tallo se enrolla en milisegundos; el zooide cae
    // hacia el disco. El tirón mueve el agua (lo sienten los vecinos). En Zoothamnium el espasmonema es
    // continuo por todas las ramas: se contrae la colonia entera.
    if (!registry_.valid(anchor.rod))
        return;
    Stalk* st = registry_.try_get<Stalk>(anchor.rod);
    if (!st)
        return;
    const StalkSpec& spec = speciesInfo(st->species).stalk;
    if (spec.kind != StalkKind::Spasmoneme || st->rod.contracted > 0.5f || st->rod.nodes.size() < 2)
        return;
    if (spec.sharedSpasmoneme) {
        const entt::entity root = registry_.valid(st->root) ? st->root : anchor.rod;
        for (auto [se, other] : registry_.view<Stalk>().each())
            if (se != anchor.rod && (se == root || other.root == root))
                other.rod.contracted = 1.0f;
    }
    const float before = stalkRestLength(st->rod, spec);
    st->rod.contracted = 1.0f;
    const float shortening = before - stalkRestLength(st->rod, spec);
    const Vec2 a = st->rod.nodes.front(), b = st->rod.nodes.back();
    const Vec2 d = b - a;
    const float len = length(d);
    if (len > 1e-3f && shortening > 1.0f)
        contractionKicks_.push_back({(a + b) * 0.5f, d * (1.0f / len), 0.5f * len, 0.5f * shortening / dt,
                                     std::max(4.0f, 2.0f * spec.width)});
}

void World::stepStalks(float dt) {
    PROFILE_SCOPE("World::stepStalks");
    const float bioDt = dt * bioScale();
    const float hours = bioDt / 3600.0f;
    // Con hambre, el zooide de algunas especies arma su corona de cilios posterior y se va nadando
    // (telotroco migratorio de los peritricos; Stentor suelta el pie). Deja el tallo.
    detaching_.clear();
    for (auto [e, anchor, meta] : registry_.view<Anchored, Metabolism>().each()) {
        const StalkSpec& spec = infoOfEntity(registry_, e).stalk;
        if (!spec.detaches || !meta.starving || registry_.any_of<Dividing, Cyst>(e))
            continue;
        const RingBody* body = registry_.try_get<RingBody>(e);
        Rng rng(config_.seed, rng_stream::kStalk, body ? body->id : 0u, step_);
        if (rng.uniform() < 0.5f * hours) // ~media hora biológica de hambre (estimado)
            detaching_.push_back(e);
    }
    for (entt::entity e : detaching_) {
        registry_.remove<Anchored>(e);
        registry_.emplace_or_replace<Telotroch>(e, Telotroch{60.0f});
    }

    stalkGone_.clear();
    // De la base hacia las ramas: cada rama sale de la punta de su padre ya resuelta en este paso.
    stalkOrder_.clear();
    for (auto [se, st] : registry_.view<Stalk>().each())
        stalkOrder_.push_back({st.depth, se});
    std::sort(stalkOrder_.begin(), stalkOrder_.end(),
              [](const auto& a, const auto& b) { return a.first != b.first ? a.first < b.first : a.second < b.second; });
    for (const auto& [depth, se] : stalkOrder_) {
        Stalk& st = registry_.get<Stalk>(se);
        const StalkSpec& spec = speciesInfo(st.species).stalk;
        StalkRod& rod = st.rod;
        if (registry_.valid(st.parent)) // rama de una colonia: su base va con la punta del padre
            if (const Stalk* parent = registry_.try_get<Stalk>(st.parent); parent && parent->rod.nodes.size() >= 2) {
                const Vec2 tip = parent->rod.nodes.back(), prev = parent->rod.nodes[parent->rod.nodes.size() - 2];
                Vec2 d = tip - prev;
                const float dl = length(d);
                d = dl > 1e-3f ? d * (1.0f / dl) : parent->rod.baseDirection;
                rod.baseDirection = {d.x * std::cos(st.turn) - d.y * std::sin(st.turn), d.x * std::sin(st.turn) + d.y * std::cos(st.turn)};
                rod.nodes[0] = tip;
            }
        // ¿Sigue el zooide? Si murió o se fue, el tallo queda vacío en el vidrio.
        const Vec2* tip = nullptr;
        Vec2 pole;
        Anchored* anchor = nullptr;
        if (registry_.valid(st.zooid))
            if (Anchored* an = registry_.try_get<Anchored>(st.zooid); an && an->rod == se)
                if (const RingBody* body = registry_.try_get<RingBody>(st.zooid)) {
                    anchor = an;
                    const std::uint32_t s = posteriorSlot(st.zooid, *body);
                    const Vec2 last = rod.nodes.back();
                    pole = last + Vec2{minImage(bodies_.x[s] - last.x, width_), minImage(bodies_.y[s] - last.y, height_)};
                    tip = &pole;
                }
        if (!anchor) {
            st.zooid = entt::null;
            st.emptyHours += hours;
            rod.contracted = 0.0f; // sin zooide el espasmonema queda relajado
            if (st.emptyHours > spec.decayHours && st.children == 0) { // con ramas vivas, sigue sosteniéndolas
                stalkGone_.push_back(se);
                continue;
            }
        } else {
            // Secreción del tallo después de fijarse (horas biológicas).
            if (spec.growthHours > 0.0f && rod.length < spec.length)
                rod.length = std::min(spec.length, rod.length + spec.length * hours / spec.growthHours);
            // Contracciones espontáneas (Vorticella se contrae cada tanto aunque nada la toque).
            if (spec.kind == StalkKind::Spasmoneme && spec.spontaneousPerMinute > 0.0f) {
                Rng rng(config_.seed, rng_stream::kStalk + 1, st.id, step_);
                if (rng.uniform() < spec.spontaneousPerMinute * dt / 60.0f)
                    contractStalk(*anchor, dt);
            }
        }
        rod.contracted = std::max(0.0f, rod.contracted - dt / spec.extensionSeconds);
        // Agua en cada nodo: el tallo es pasivo, siente el flujo total (también las corrientes de los
        // filtradores).
        stalkFlow_.resize(rod.nodes.size());
        for (std::size_t i = 0; i < rod.nodes.size(); ++i)
            stalkFlow_[i] = fluid_.velocityAt(rod.nodes[i]);
        stepStalk(rod, spec, stalkFlow_, tip, dt);
        if (anchor) {
            anchor->stalk = stalkRestLength(rod, spec);
            anchor->contracted = rod.contracted;
        }
    }
    for (entt::entity se : stalkGone_) {
        if (const Stalk* st = registry_.try_get<Stalk>(se); st && registry_.valid(st->parent))
            if (Stalk* parent = registry_.try_get<Stalk>(st->parent))
                --parent->children;
        registry_.destroy(se);
    }
}
