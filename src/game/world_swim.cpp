// Recorrido pasivo de los nadadores que no eligen rumbo ([being.swim_path]): el espermatozoide no tiene
// órganos de los sentidos ni reacción de evitación; su trayectoria es la de su batido, que la curva un
// poco y fluctúa. Lo demás (choques, la pared, otras células) lo resuelve la mecánica.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "game/reagent.hpp"

#include <cmath>

namespace {
SpeciesId speciesOfEntity(const entt::registry& reg, entt::entity e) {
    if (const Species* sp = reg.try_get<Species>(e))
        return sp->id;
    return playerSpecies(reg.get<Locomotion>(e).type);
}
} // namespace

void World::steerSwimmer(entt::entity e, const RingBody& body, Locomotion& loco, float dt) {
    PROFILE_SCOPE("World::steerSwimmer");
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    const SwimPathSpec& spec = info.swimPath;
    SwimState* st = registry_.try_get<SwimState>(e);
    if (!st) { // el sentido de la curva, propio de cada individuo
        Rng first(config_.seed, rng_stream::kSwim, body.id, 0);
        st = &registry_.emplace<SwimState>(e);
        st->chirality = first.uniform() < 0.5f ? -1.0f : 1.0f;
    }
    Rng rng(config_.seed, rng_stream::kSwim, body.id, step_ + 1);
    const AnimalCell* ac = registry_.try_get<AnimalCell>(e);
    const bool hyper = ac && ac->hyperactive;
    // Ornstein–Uhlenbeck: dX = −X/τ dt + σ·√(2/τ) dW, con varianza σ² y difusión del ángulo D = σ²·τ. El
    // hiperactivado cambia de rumbo mucho más (trayectorias en estrella).
    const float tau = spec.noiseSeconds;
    const float sigma = std::sqrt((hyper ? info.capacitation.hyperRotationalDiffusion : spec.rotationalDiffusion) / tau);
    st->turnNoise += -st->turnNoise * (dt / tau) + sigma * std::sqrt(2.0f * dt / tau) * rng.normal();
    const float speed = maxSpeedOf(locomotionParams_, loco.type) * loco.power;
    // Los no progresivos (OMS) baten sin avanzar: dan vueltas en círculos chicos; el hiperactivado, también.
    float radius = ac && info.motility.present && ac->grade == MotilityGrade::NonProgressive
                       ? info.motility.nonProgressiveRadius
                       : spec.curvatureRadius;
    if (hyper)
        radius = info.capacitation.hyperCurvatureRadius;
    float curve = radius > 0.0f ? st->chirality * speed / radius : 0.0f;
    // Quimiotaxis a la progesterona (capacitado): compara la señal de ahora con la de hace ~1 s. Si sube,
    // sigue más derecho; si baja, gira (giro-y-avance; Alvarez et al. 2014, Trends Cell Biol. 24: 198).
    const CapacitationSpec& cp = info.capacitation;
    if (ac && cp.present && cp.chemotaxisThreshold > 0.0f && ac->capacitation >= 1.0f && reagentIndex_ >= 0 &&
        reagent(reagentIndex_).key == cp.inducer) {
        const float c = std::max(0.0f, reagent_.grid().sample(body.centroid.x, body.centroid.y));
        if (c > cp.chemotaxisThreshold) {
            const float change = c - st->signalMemory;
            if (change > 0.0f)
                curve *= 0.3f; // mejora: menos curva
            else if (st->turnBout <= 0.0f && change < -0.02f * c)
                st->turnBout = 0.5f; // empeora: un giro cerrado de medio segundo
        }
        st->signalMemory += (c - st->signalMemory) * std::min(1.0f, dt / 1.0f);
    }
    if (st->turnBout > 0.0f) {
        st->turnBout -= dt;
        curve = st->chirality * locomotionParams_.flagellumTurnRate * 3.0f; // gira al máximo
    }
    float omega = curve + st->turnNoise; // [rad/s]
    if (spec.rheotaxisAlignment > 0.0f) {
        // Reotaxis: la corriente de fondo (la que siente, sin su propio dipolo) es la media de la película;
        // en un flujo de Poiseuille entre dos vidrios la cizalla en la pared es 6·u/h.
        const Vec2 u = fluid_.backgroundAt(body.centroid);
        const float flow = length(u);
        if (flow > 1e-3f) {
            const float shear = 6.0f * flow / std::max(1.0f, fluid_.gapHeight);
            const Vec2 up = u * (-1.0f / flow);
            const Vec2 h = {std::cos(body.angle), std::sin(body.angle)};
            omega += shear * (spec.rheotaxisAlignment * cross(h, up) + spec.rheotaxisChirality);
        }
    }
    // El flagelo gira el cuerpo con ω = 3·Δ hacia el objetivo (bio/locomotion): el objetivo va a ω/3 del
    // rumbo actual, así el giro es el del batido y no depende de adónde quedó apuntando tras un choque.
    const float a = body.angle + omega / 3.0f;
    loco.target = body.centroid + Vec2{std::cos(a), std::sin(a)} * 100.0f;
    loco.active = true;
}
