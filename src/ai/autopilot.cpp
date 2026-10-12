#include "ai/autopilot.hpp"

#include "bio/locomotion.hpp"
#include "bio/species.hpp"
#include "core/units.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {

float clampTurn(float t) { return std::clamp(t, -1.0f, 1.0f); }

// Giro (−1..1) para orientar el eje `h` hacia `to`.
float turnToward(Vec2 h, Vec2 to) {
    const float len = length(to);
    if (len < 1e-4f)
        return 0.0f;
    const Vec2 u = to * (1.0f / len);
    return clampTurn(2.0f * std::atan2(cross(h, u), dot(h, u)));
}

} // namespace

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, const SpeciesInfo& species,
                    const Perception& sense, Rng& rng, float dt) {
    const Vec2 h = bodyHeading(body);

    switch (loco.type) {
    case LocomotionType::Cilia: {
        loco.active = false;
        // Reacción de evitación: ante una amenaza o un choque del polo anterior (mecanorrecepción),
        // la ciliatura se invierte, la célula retrocede y gira.
        const bool preyAhead = sense.hasPrey && sense.preyDistance < 10.0f;
        if (((sense.hasThreat && sense.threatDistance < 40.0f) || (sense.bumped && !preyAhead)) &&
            pilot.reverse <= 0.0f) {
            pilot.reverse = 1.0f;
            pilot.turn = rng.uniform() < 0.5f ? -1.0f : 1.0f;
        }
        if (pilot.reverse > 0.0f) {
            pilot.reverse -= dt;
            loco.forward = -1.0f;
            loco.turn = pilot.turn;
            return;
        }
        // Depredador: persigue a la presa que detecta (mecanorrecepción, corto alcance).
        if (sense.hasPrey && sense.preyDistance < 150.0f) {
            loco.forward = 1.0f;
            loco.turn = turnToward(h, sense.prey - body.centroid);
            return;
        }
        // Quimiocinesis temporal: compara la comida de ahora con la de hace ~1 s.
        float noise = 0.9f;
        if (species.chemokinesis) {
            const float change = sense.food - pilot.memory;
            pilot.memory += (sense.food - pilot.memory) * std::min(1.0f, dt);
            noise = change > 0.0f ? 0.3f : 1.5f;
        }
        pilot.turn = clampTurn(pilot.turn - pilot.turn * dt + noise * std::sqrt(dt) * rng.normal());
        loco.forward = 0.6f;
        loco.turn = pilot.turn;
        return;
    }
    case LocomotionType::Flagellum: {
        loco.active = true;
        if (sense.bumped) { // chocó: cambia de rumbo
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * 300.0f;
            pilot.retarget = rng.uniform(3.0f, 8.0f);
        }
        if (sense.hasThreat && sense.threatDistance < 60.0f) { // huye en línea recta
            loco.target = body.centroid + (body.centroid - sense.threat) * 4.0f;
            return;
        }
        pilot.retarget -= dt;
        if (species.phototaxis && std::abs(sense.lightLeft - sense.lightRight) > 0.02f) {
            // Fototaxis: el estigma sombrea el fotorreceptor; gira hacia el lado más iluminado.
            const Vec2 side = {-h.y, h.x};
            loco.target = body.centroid + (h + side * (sense.lightLeft > sense.lightRight ? 0.5f : -0.5f)) * 200.0f;
            pilot.retarget = 0.0f;
            return;
        }
        if (pilot.retarget <= 0.0f) {
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * 300.0f;
            pilot.retarget = rng.uniform(3.0f, 8.0f);
        }
        loco.target = pilot.target;
        return;
    }
    case LocomotionType::Pseudopod: {
        loco.active = true;
        if (sense.hasPrey && sense.preyDistance < 300.0f) { // quimiotaxis hacia la presa
            loco.target = sense.prey;
            return;
        }
        pilot.retarget -= dt;
        if (pilot.retarget <= 0.0f) {
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * 150.0f;
            pilot.retarget = rng.uniform(20.0f, 40.0f);
        }
        loco.target = pilot.target;
        return;
    }
    }
}
