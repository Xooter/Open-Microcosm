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

// Respuestas fóbicas temporales (M1) y mecanoestímulo (M2): compara el nivel de ahora con el recordado.
// Devuelve la acción disparada en este paso, si hay una.
enum class Fired { None, Avoid, Jump };
Fired evaluateResponses(Autopilot& pilot, const SpeciesInfo& species, const Perception& sense, float dt) {
    pilot.refractory = std::max(0.0f, pilot.refractory - dt);
    Fired fired = Fired::None;
    const std::size_t n = std::min(species.responses.size(), pilot.responseMemory.size());
    for (std::size_t k = 0; k < n; ++k) {
        const ResponseSpec& r = species.responses[k];
        const float level = r.field == ResponseField::Light    ? sense.light
                            : r.field == ResponseField::Oxygen ? sense.oxygen
                                                               : sense.shear;
        float& memory = pilot.responseMemory[k];
        if (memory < 0.0f)
            memory = level;
        const float scale = std::max(memory, 1e-3f);
        bool fire = false;
        switch (r.kind) {
        case ResponseKind::StepUp: fire = level > r.minLevel && level - memory > r.threshold * scale; break;
        case ResponseKind::StepDown: fire = memory > r.minLevel && memory - level > r.threshold * scale; break;
        case ResponseKind::Above: fire = level > r.threshold; break;
        }
        memory += (level - memory) * std::min(1.0f, dt / r.memory);
        if (fire && pilot.refractory <= 0.0f && fired == Fired::None) {
            fired = r.action == ResponseAction::Jump ? Fired::Jump : Fired::Avoid;
            pilot.refractory = r.refractory;
        }
    }
    return fired;
}

} // namespace

void steerAutopilot(Autopilot& pilot, Locomotion& loco, const RingBody& body, const SpeciesInfo& species,
                    const Perception& sense, Rng& rng, float dt) {
    const Vec2 h = bodyHeading(body);
    const Fired fired = evaluateResponses(pilot, species, sense, dt);
    loco.burst = 0.0f;

    // Saltos (Halteria): espontáneos cada tanto y de escape ante la corriente de un depredador o una
    // amenaza cerca (Gilbert 1994: el salto la saca del alcance de los rotíferos).
    if (const JumpSpec& jump = species.jump; jump.speed > 0.0f) {
        bool start = fired == Fired::Jump || (sense.hasThreat && sense.threatDistance < 40.0f);
        if (jump.interval > 0.0f) {
            if (pilot.nextJump < 0.0f)
                pilot.nextJump = -jump.interval * std::log(1.0f - 0.999f * rng.uniform());
            pilot.nextJump -= dt;
            if (pilot.nextJump <= 0.0f) {
                start = true;
                pilot.nextJump = -jump.interval * std::log(1.0f - 0.999f * rng.uniform());
            }
        }
        if (start && pilot.jump <= 0.0f) {
            pilot.jump = jump.duration;
            pilot.turn = rng.uniform(-1.0f, 1.0f); // después del salto se reorienta
        }
        if (pilot.jump > 0.0f) {
            pilot.jump -= dt;
            loco.burst = jump.speed;
            loco.forward = 1.0f;
            loco.turn = 0.0f;
            loco.active = true;
            pilot.intent = Intent::Jump;
            return;
        }
    }

    switch (loco.type) {
    case LocomotionType::Cilia: {
        loco.active = false;
        if (fired == Fired::Avoid && pilot.reverse <= 0.0f) { // respuesta fóbica: reacción de evitación
            pilot.intent = Intent::Phobic;
            pilot.reverse = 1.0f;
            pilot.turn = rng.uniform() < 0.5f ? -1.0f : 1.0f;
        }
        // Reacción de evitación: ante una amenaza o un choque del polo anterior (mecanorrecepción),
        // la ciliatura se invierte, la célula retrocede y gira.
        const bool preyAhead = sense.hasPrey && sense.preyDistance < 10.0f;
        if (((sense.hasThreat && sense.threatDistance < 40.0f) || (sense.bumped && !preyAhead)) &&
            pilot.reverse <= 0.0f) {
            pilot.intent = sense.hasThreat && sense.threatDistance < 40.0f ? Intent::AvoidThreat : Intent::AvoidBump;
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
            pilot.intent = Intent::Chase;
            loco.forward = 1.0f;
            loco.turn = turnToward(h, sense.prey - body.centroid);
            return;
        }
        // Trampa de ácido (Jennings, 1906): el paramecio entra libremente a una zona con CO₂,
        // pero al salir hacia agua menos ácida hace la reacción de evitación y vuelve a entrar.
        // Su propio CO₂ agranda la zona: así se forman los enjambres.
        if (species.acidTrap) {
            const float change = sense.acid - pilot.acidMemory;
            pilot.acidMemory += change * std::min(1.0f, 2.0f * dt);
            if (change < -0.01f && sense.acid > 0.02f) {
                pilot.intent = Intent::AcidTrap;
                pilot.reverse = 0.4f;
                pilot.turn = rng.uniform() < 0.5f ? -1.0f : 1.0f;
                loco.forward = -1.0f;
                loco.turn = pilot.turn;
                return;
            }
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
        pilot.intent = Intent::Wander;
        if (species.glider) { // diatomea: se desliza por el rafe a lo largo de su eje, con inversiones
            pilot.retarget -= dt;
            if (pilot.retarget <= 0.0f) {
                pilot.walking = !pilot.walking;
                pilot.retarget = rng.uniform(5.0f, 20.0f);
            }
            loco.forward = pilot.walking ? 1.0f : -1.0f;
            loco.turn = 0.1f * rng.normal();
            pilot.intent = Intent::Glide;
            return;
        }
        if (species.walker) { // Euplotes: camina sobre los cirros con paradas bruscas
            pilot.retarget -= dt;
            if (pilot.retarget <= 0.0f) {
                pilot.walking = !pilot.walking;
                pilot.retarget = pilot.walking ? rng.uniform(0.5f, 2.0f) : rng.uniform(0.3f, 1.0f);
                pilot.turn = rng.uniform(-1.0f, 1.0f);
            }
            loco.forward = pilot.walking ? 0.8f : 0.0f;
            pilot.intent = pilot.walking ? Intent::Walk : Intent::Pause;
        }
        return;
    }
    case LocomotionType::Flagellum: {
        loco.active = true;
        // Respuesta fotofóbica (Euglena, Chlamydomonas): ante un aumento brusco de la luz se detiene,
        // gira y sale en otra dirección; mientras dura, no hace fototaxis.
        if (fired == Fired::Avoid) {
            const float a = rng.uniform(2.0f, 4.3f); // hacia atrás, con desvío
            pilot.target = body.centroid + Vec2{h.x * std::cos(a) - h.y * std::sin(a), h.x * std::sin(a) + h.y * std::cos(a)} * 300.0f;
            pilot.reverse = 1.5f;
            pilot.retarget = rng.uniform(3.0f, 8.0f);
        }
        if (pilot.reverse > 0.0f) {
            pilot.reverse -= dt;
            loco.target = pilot.target;
            pilot.intent = Intent::Phobic;
            return;
        }
        pilot.intent = Intent::Wander;
        if (sense.bumped) { // chocó: cambia de rumbo
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * 300.0f;
            pilot.retarget = rng.uniform(3.0f, 8.0f);
            pilot.intent = Intent::AvoidBump;
        }
        if (sense.hasThreat && sense.threatDistance < 60.0f) { // huye en línea recta
            loco.target = body.centroid + (body.centroid - sense.threat) * 4.0f;
            pilot.intent = Intent::Flee;
            return;
        }
        pilot.retarget -= dt;
        if (species.phototaxis && std::abs(sense.lightLeft - sense.lightRight) > 0.02f) {
            // Fototaxis: el estigma sombrea el fotorreceptor; gira hacia el lado más iluminado.
            const Vec2 side = {-h.y, h.x};
            loco.target = body.centroid + (h + side * (sense.lightLeft > sense.lightRight ? 0.5f : -0.5f)) * 200.0f;
            pilot.retarget = 0.0f;
            pilot.intent = Intent::Phototaxis;
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
            pilot.intent = Intent::Chase;
            return;
        }
        pilot.intent = Intent::Wander;
        pilot.retarget -= dt;
        if (pilot.retarget <= 0.0f) {
            const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
            pilot.target = body.centroid + Vec2{std::cos(angle), std::sin(angle)} * 150.0f;
            pilot.retarget = rng.uniform(20.0f, 40.0f);
        }
        loco.target = pilot.target;
        return;
    }
    case LocomotionType::Crawl: return; // las células animales las guía el mundo (world_animal)
    case LocomotionType::None: return;
    }
}
