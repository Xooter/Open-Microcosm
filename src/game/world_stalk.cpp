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

void World::contractStalk(Anchored& anchor, float dt) {
    // Espasmonema: el Ca²⁺ dispara la espasmina y el tallo se enrolla en milisegundos; el zooide cae
    // hacia el disco. El tirón mueve el agua (lo sienten los vecinos).
    if (!registry_.valid(anchor.rod))
        return;
    Stalk* st = registry_.try_get<Stalk>(anchor.rod);
    if (!st)
        return;
    const StalkSpec& spec = speciesInfo(st->species).stalk;
    if (spec.kind != StalkKind::Spasmoneme || st->rod.contracted > 0.5f || st->rod.nodes.size() < 2)
        return;
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
    for (auto [se, st] : registry_.view<Stalk>().each()) {
        const StalkSpec& spec = speciesInfo(st.species).stalk;
        StalkRod& rod = st.rod;
        // ¿Sigue el zooide? Si murió o se fue, el tallo queda vacío en el vidrio.
        const Vec2* tip = nullptr;
        Vec2 pole;
        Anchored* anchor = nullptr;
        if (registry_.valid(st.zooid))
            if (Anchored* an = registry_.try_get<Anchored>(st.zooid); an && an->rod == se)
                if (const RingBody* body = registry_.try_get<RingBody>(st.zooid)) {
                    anchor = an;
                    const std::uint32_t s = body->ring[poleIndex(*body, false)];
                    const Vec2 last = rod.nodes.back();
                    pole = last + Vec2{minImage(bodies_.x[s] - last.x, width_), minImage(bodies_.y[s] - last.y, height_)};
                    tip = &pole;
                }
        if (!anchor) {
            st.zooid = entt::null;
            st.emptyHours += hours;
            rod.contracted = 0.0f; // sin zooide el espasmonema queda relajado
            if (st.emptyHours > spec.decayHours) {
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
    for (entt::entity se : stalkGone_)
        registry_.destroy(se);
}
