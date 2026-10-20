// Onda de forma (ShapeWaveSpec): la metabolia de los euglenoides y el vaivén de la probóscide de Dileptus.
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "game/rest_shape.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
const SpeciesInfo& infoOf(const entt::registry& reg, entt::entity e) {
    static const SpeciesInfo kNone;
    const Species* sp = reg.try_get<Species>(e);
    return sp ? speciesInfo(sp->id) : kNone;
}
// La metabolia entra y sale de a poco: la onda crece y se apaga en unos segundos.
constexpr float kMetabolyFade = 2.0f; // [s]
} // namespace

void World::updateShapeWave(entt::entity e, RingBody& body, Locomotion& loco, float dt) {
    PROFILE_SCOPE("World::updateShapeWave");
    const ShapeWaveSpec& spec = infoOf(registry_, e).shapeWave;
    if (spec.kind == ShapeWaveKind::None)
        return;
    ShapeWave& w = registry_.get_or_emplace<ShapeWave>(e);
    // Dividiéndose o en quiste, la forma la llevan la división o la pared; después se toma la nueva.
    if (registry_.any_of<Dividing, Cyst>(e)) {
        w.base.clear();
        w.left = 0.0f;
        return;
    }
    if (w.base.size() != body.restRing.size()) { // recién creada o recién dividida: sin deformar
        w.base = body.restRing;
        w.left = 0.0f;
        w.time = 0.0f;
    }
    float envelope = 1.0f;
    if (spec.kind == ShapeWaveKind::Metaboly) {
        if (w.left <= 0.0f && spec.episodesPerMinute > 0.0f) {
            Rng rng(config_.seed, rng_stream::kShapeWave, body.id, step_);
            if (rng.uniform() < spec.episodesPerMinute / 60.0f * dt) {
                w.left = spec.episodeSeconds;
                w.time = 0.0f;
            }
        }
        if (w.left <= 0.0f)
            return; // nada: la forma de reposo es la de siempre
        w.left = std::max(0.0f, w.left - dt);
        w.time += dt;
        envelope = std::clamp(std::min(w.time, w.left) / kMetabolyFade, 0.0f, 1.0f);
        // Mientras cambia de forma no nada (el flagelo de Euglena puede seguir batiendo, pero no avanza).
        loco.forward = loco.turn = 0.0f;
        loco.active = false;
    } else {
        w.time += dt;
    }
    float x0 = 1e9f, x1 = -1e9f;
    for (const Vec2& q : w.base) {
        x0 = std::min(x0, q.x);
        x1 = std::max(x1, q.x);
    }
    const float span = std::max(1e-3f, x1 - x0);
    const float phase = 2.0f * units::kPi * w.time / spec.periodSeconds;
    thread_local std::vector<Vec2> shape;
    shape = w.base;
    for (Vec2& q : shape) {
        const float u = (q.x - x0) / span; // 0 en el polo posterior, 1 en el anterior
        if (spec.kind == ShapeWaveKind::Metaboly) {
            // El ensanchamiento corre del polo anterior al posterior.
            q.y *= 1.0f + spec.amplitude * envelope * std::sin(2.0f * units::kPi * u / spec.wavelength + phase);
        } else {
            // La parte anterior se dobla hacia un lado y el otro; más cuanto más cerca de la punta.
            const float s = std::clamp((u - (1.0f - spec.reach)) / spec.reach, 0.0f, 1.0f);
            q.y += spec.amplitude * 0.5f * span * s * s * std::sin(phase);
        }
    }
    rest_shape::apply(body, shape);
}

void World::relaxShapeWave(entt::entity e, RingBody& body) {
    // Antes de dividirse vuelve a la forma sin deformar (la división parte de la forma de la madre).
    ShapeWave* w = registry_.try_get<ShapeWave>(e);
    if (!w || w->base.size() != body.restRing.size())
        return;
    rest_shape::apply(body, w->base);
    w->left = 0.0f;
    w->base.clear();
}
