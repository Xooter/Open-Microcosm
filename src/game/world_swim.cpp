// Recorrido pasivo de los nadadores que no eligen rumbo ([being.swim_path]): el espermatozoide no tiene
// órganos de los sentidos ni reacción de evitación; su trayectoria es la de su batido, que la curva un
// poco y fluctúa. Lo demás (choques, la pared, otras células) lo resuelve la mecánica.
#include "game/world.hpp"

#include "core/profiling.hpp"

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
    // Ornstein–Uhlenbeck: dX = −X/τ dt + σ·√(2/τ) dW, con varianza σ² y difusión del ángulo D = σ²·τ.
    const float tau = spec.noiseSeconds;
    const float sigma = std::sqrt(spec.rotationalDiffusion / tau);
    st->turnNoise += -st->turnNoise * (dt / tau) + sigma * std::sqrt(2.0f * dt / tau) * rng.normal();
    const float speed = maxSpeedOf(locomotionParams_, loco.type) * loco.power;
    // Los no progresivos (OMS) baten sin avanzar: dan vueltas en círculos chicos.
    const AnimalCell* ac = registry_.try_get<AnimalCell>(e);
    const float radius = ac && info.motility.present && ac->grade == MotilityGrade::NonProgressive
                             ? info.motility.nonProgressiveRadius
                             : spec.curvatureRadius;
    const float curve = radius > 0.0f ? st->chirality * speed / radius : 0.0f;
    const float omega = curve + st->turnNoise; // [rad/s]
    // El flagelo gira el cuerpo con ω = 3·Δ hacia el objetivo (bio/locomotion): el objetivo va a ω/3 del
    // rumbo actual, así el giro es el del batido y no depende de adónde quedó apuntando tras un choque.
    const float a = body.angle + omega / 3.0f;
    loco.target = body.centroid + Vec2{std::cos(a), std::sin(a)} * 100.0f;
    loco.active = true;
}
