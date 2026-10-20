// Conjugación escalariforme de Spirogyra: con el nitrógeno limitando (la señal real de la reproducción
// sexual en las zignematáceas), dos filamentos de tipos de apareamiento distintos que quedan lado a lado
// se emparejan célula a célula: las células enfrentadas forman papilas que se unen en un tubo (el
// conjunto parece una escalera), el protoplasto de la célula donante pasa por el tubo a la receptora y
// los dos se funden en una cigospora de pared gruesa, que queda latente; la donante queda como una pared
// vacía en su filamento (Kreutz; Transeau 1951, The Zygnemataceae; Lee 2018, Phycology).
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Separación entre las paredes de las dos células emparejadas, a la que se acercan.
constexpr float kGap = 8.0f; // [µm]
// Tasa con que los filamentos se acercan (sin inercia, como las colonias).
constexpr float kApproachRate = 1.5f; // [1/s]

float halfWidth(const RingBody& b) {
    float w = 0.0f;
    for (const Vec2& q : b.restRing)
        w = std::max(w, std::abs(q.y));
    return w;
}
} // namespace

void World::updateAlgalConjugation(float dt) {
    PROFILE_SCOPE("World::updateAlgalConjugation");
    const float hours = dt * bioScale() / 3600.0f;
    // 1. Parejas nuevas: una vez por segundo de simulación alcanza (el proceso dura horas).
    if (step_ % 60 == 0) {
        thread_local std::vector<entt::entity> ready;
        ready.clear();
        for (auto [e, sp, meta, her] : registry_.view<Species, Metabolism, Heredity>().each()) {
            const SpeciesInfo& info = speciesInfo(sp.id);
            if (info.algalPairReach <= 0.0f || meta.limiting != Limitation::Nitrogen ||
                registry_.any_of<AlgalPair, Zygospore, EmptyWall, Dividing, Cyst>(e) || !registry_.all_of<RingBody>(e))
                continue;
            ready.push_back(e);
        }
        for (std::size_t i = 0; i < ready.size(); ++i) {
            const entt::entity a = ready[i];
            if (registry_.all_of<AlgalPair>(a))
                continue;
            const RingBody& ba = registry_.get<RingBody>(a);
            const float mateA = registry_.get<Heredity>(a).genome[Gene::MatingType];
            const SpeciesId spA = registry_.get<Species>(a).id;
            const float reach = speciesInfo(spA).algalPairReach;
            const Vec2 ha = bodyHeading(ba);
            entt::entity best = entt::null;
            float bestDist = 1e9f;
            for (std::size_t j = 0; j < ready.size(); ++j) {
                const entt::entity b = ready[j];
                if (b == a || registry_.all_of<AlgalPair>(b) || registry_.get<Species>(b).id != spA ||
                    registry_.get<Heredity>(b).genome[Gene::MatingType] == mateA)
                    continue;
                const RingBody& bb = registry_.get<RingBody>(b);
                const Vec2 d = bb.centroid + imageShift(bb, ba.centroid) - ba.centroid;
                const float dist = length(d);
                const float gap = dist - halfWidth(ba) - halfWidth(bb);
                // Lado a lado: ejes casi paralelos y la otra célula de costado, no adelante.
                if (gap > reach || dist < 1e-3f || std::abs(dot(ha, bodyHeading(bb))) < 0.85f ||
                    std::abs(cross(ha, d)) < 0.7f * dist)
                    continue;
                if (dist < bestDist) {
                    bestDist = dist;
                    best = b;
                }
            }
            if (best == entt::null)
                continue;
            // La donante es la de tipo "+" (en la realidad, todo un filamento hace de donante).
            const bool aDonor = mateA > 0.5f;
            registry_.emplace<AlgalPair>(a, AlgalPair{best, aDonor, 0.0f});
            registry_.emplace<AlgalPair>(best, AlgalPair{a, !aDonor, 0.0f});
        }
    }

    // 2. Parejas en curso.
    thread_local std::vector<entt::entity> broken, done;
    broken.clear();
    done.clear();
    for (auto [e, pair] : registry_.view<AlgalPair>().each()) {
        if (!registry_.valid(pair.partner) || !registry_.all_of<AlgalPair, RingBody>(pair.partner) ||
            !registry_.all_of<RingBody>(e)) {
            broken.push_back(e); // se murió o se la comieron: la otra queda sola
            continue;
        }
        if (!pair.donor)
            continue; // cada pareja la lleva la donante
        AlgalPair& other = registry_.get<AlgalPair>(pair.partner);
        RingBody& a = registry_.get<RingBody>(e);
        RingBody& b = registry_.get<RingBody>(pair.partner);
        const SpeciesInfo& info = speciesOfCell(e);
        // Se acercan hasta quedar a la distancia del tubo (los filamentos se curvan hacia la pareja).
        const Vec2 d = b.centroid + imageShift(b, a.centroid) - a.centroid;
        const float dist = std::max(1e-3f, length(d));
        const Vec2 u = d * (1.0f / dist);
        const float want = halfWidth(a) + halfWidth(b) + kGap;
        // Si se separan antes de que el tubo esté cerrado, la pareja se pierde (las papilas no se unen).
        if (dist - want > 2.0f * info.algalPairReach &&
            pair.progress < info.algalTubeHours / (info.algalTubeHours + info.algalTransferHours + info.algalZygoteHours)) {
            broken.push_back(e);
            broken.push_back(pair.partner);
            continue;
        }
        const Vec2 v = u * ((dist - want) * 0.5f * kApproachRate);
        for (std::uint32_t s : a.ring) {
            bodies_.vx[s] += v.x;
            bodies_.vy[s] += v.y;
        }
        for (std::uint32_t s : b.ring) {
            bodies_.vx[s] -= v.x;
            bodies_.vy[s] -= v.y;
        }
        // Tubo, pasaje y cigospora, en horas biológicas.
        const float tube = info.algalTubeHours, transfer = info.algalTransferHours, zygote = info.algalZygoteHours;
        const float total = tube + transfer + zygote;
        const float before = pair.progress;
        pair.progress = std::min(1.0f, pair.progress + hours / total);
        other.progress = pair.progress;
        const float t0 = tube / total, t1 = (tube + transfer) / total;
        // Pasaje del protoplasto: la fracción que corresponde a este paso de lo que le queda a la donante.
        if (pair.progress > t0 && before < t1) {
            const float from = std::max(before, t0), to = std::min(pair.progress, t1);
            const float share = std::clamp((to - from) / std::max(1e-6f, t1 - from), 0.0f, 1.0f);
            Metabolism* md = registry_.try_get<Metabolism>(e);
            Metabolism* mr = registry_.try_get<Metabolism>(pair.partner);
            if (md && mr) {
                auto move = [&](float& give, float& take) {
                    const float x = give * share;
                    give -= x;
                    take += x;
                };
                move(md->biomass, mr->biomass);
                move(md->reserves, mr->reserves);
                move(md->phosphorus, mr->phosphorus);
                move(md->nitrogen, mr->nitrogen);
                move(md->silicon, mr->silicon);
                move(md->iron, mr->iron);
                move(md->vitamin, mr->vitamin);
            }
        }
        if (pair.progress >= 1.0f)
            done.push_back(e);
    }
    for (entt::entity e : broken)
        registry_.remove<AlgalPair>(e);
    for (entt::entity e : done) {
        const entt::entity r = registry_.get<AlgalPair>(e).partner;
        // La donante queda como pared vacía (sin nada adentro: lo que quedara pasó a la receptora).
        if (Metabolism* md = registry_.try_get<Metabolism>(e); md && registry_.all_of<Metabolism>(r)) {
            Metabolism& mr = registry_.get<Metabolism>(r);
            mr.biomass += md->biomass;
            mr.reserves += md->reserves;
            mr.phosphorus += md->phosphorus;
            mr.nitrogen += md->nitrogen;
            mr.silicon += md->silicon;
            mr.iron += md->iron;
            mr.vitamin += md->vitamin;
            md->biomass = md->reserves = md->phosphorus = md->nitrogen = md->silicon = md->iron = md->vitamin = 0.0f;
        }
        registry_.remove<AlgalPair>(e);
        registry_.emplace_or_replace<EmptyWall>(e);
        if (CellLook* look = registry_.try_get<CellLook>(e)) // sin cloroplasto: la pared casi incolora
            *look = CellLook{222, 224, 210};
        registry_.remove<AlgalPair>(r);
        registry_.emplace_or_replace<Zygospore>(r, Zygospore{1.0f});
    }
}
