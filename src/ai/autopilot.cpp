#include "ai/autopilot.hpp"

#include "bio/locomotion.hpp"
#include "core/units.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, Rng& rng, float dt) {
    switch (loco.type) {
    case LocomotionType::Cilia:
        // Giro como proceso de Ornstein–Uhlenbeck: cambia suave y al azar.
        pilot.turn += -pilot.turn * dt + 0.9f * std::sqrt(dt) * rng.normal();
        pilot.turn = std::clamp(pilot.turn, -1.0f, 1.0f);
        loco.forward = 0.5f;
        loco.turn = pilot.turn;
        loco.active = false;
        return;
    case LocomotionType::Flagellum:
    case LocomotionType::Pseudopod: {
        const bool amoeboid = loco.type == LocomotionType::Pseudopod;
        pilot.retarget -= dt;
        if (pilot.retarget <= 0.0f) {
            const float range = amoeboid ? 150.0f : 300.0f;
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * range;
            pilot.retarget = amoeboid ? rng.uniform(20.0f, 40.0f) : rng.uniform(3.0f, 8.0f);
        }
        loco.target = pilot.target;
        loco.active = true;
        return;
    }
    }
}
