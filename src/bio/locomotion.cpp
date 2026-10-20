#include "bio/locomotion.hpp"

#include "sim/fluid.hpp"
#include "sim/grid2d.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {

Vec2 pos(const BodyParticles& bp, std::uint32_t s) { return {bp.x[s], bp.y[s]}; }

void addVelocity(BodyParticles& bp, std::uint32_t s, Vec2 v) {
    bp.vx[s] += v.x;
    bp.vy[s] += v.y;
}

Vec2 normalized(Vec2 v) {
    const float len = length(v);
    return len > 1e-6f ? v * (1.0f / len) : Vec2{1.0f, 0.0f};
}

// Normal hacia afuera de la membrana en el punto i del anillo.
Vec2 outwardNormal(const RingBody& body, const BodyParticles& bp, std::size_t i) {
    const std::size_t n = body.ring.size();
    const Vec2 t = normalized(pos(bp, body.ring[(i + 1) % n]) - pos(bp, body.ring[(i + n - 1) % n]));
    return polygon::inwardNormal(t, body.restArea) * -1.0f;
}

// Fuerza en el agua tal que el agua bajo el punto se mueva a ~`speed` (Stokes–Brinkman).
void pushWater(Fluid& fluid, Vec2 at, Vec2 dir, float speed, float radius) {
    const float response = fluid.viscosity / (radius * radius) + fluid.brinkmanFriction();
    fluid.addForce(at, dir * (speed * response), radius);
}

float halfLength(const RingBody& body) {
    float l = 0.0f;
    for (const Vec2& q : body.restRing)
        l = std::max(l, std::abs(q.x));
    return l;
}

} // namespace

const char* locomotionName(LocomotionType type) {
    switch (type) {
    case LocomotionType::Cilia: return "cilios";
    case LocomotionType::Flagellum: return "flagelo";
    case LocomotionType::Pseudopod: return "pseudopodo";
    case LocomotionType::Crawl: return "arrastre";
    case LocomotionType::None: return "ninguna";
    }
    return "";
}

Vec2 bodyHeading(const RingBody& body) { return {std::cos(body.angle), std::sin(body.angle)}; }

void applyLocomotion(const RingBody& body, BodyParticles& bp, Locomotion& loco,
                     const LocomotionParams& p, float simTime, float width, float height) {
    const Vec2 c = body.centroid;
    const Vec2 h = bodyHeading(body);
    const Vec2 side = {-h.y, h.x};
    Vec2 linear;
    float omega = 0.0f;

    switch (loco.type) {
    case LocomotionType::Cilia: {
        const float f = loco.forward >= 0.0f ? loco.forward : loco.forward * p.ciliaReverse;
        loco.speed = f * p.ciliaSpeed * loco.power;
        omega = loco.turn * p.ciliaTurnRate;
        if (loco.burst > 0.0f) { // salto: el golpe de los cirros la lanza en línea recta
            loco.speed = loco.burst; // ya escalado por el genoma y la viscosidad (World)
            omega = 0.0f;
        }
        linear = h * loco.speed;
        break;
    }
    case LocomotionType::Flagellum: {
        loco.speed = loco.active ? p.flagellumSpeed * loco.power : 0.0f;
        if (loco.active) {
            const Vec2 to = {minImage(loco.target.x - c.x, width), minImage(loco.target.y - c.y, height)};
            const Vec2 u = normalized(to);
            const float delta = std::atan2(cross(h, u), dot(h, u));
            omega = std::clamp(3.0f * delta, -p.flagellumTurnRate, p.flagellumTurnRate);
            if (loco.gliding) {
                linear = h * loco.speed; // apoyado en el vidrio: sin bamboleo
            } else if (loco.helixRadius > 0.0f && loco.helixPeriod > 0.0f) {
                // Hélice proyectada: desplazamiento lateral R·sin(ωt), velocidad R·ω·cos(ωt).
                const float w = 6.2831853f / loco.helixPeriod;
                linear = h * loco.speed + side * (loco.helixRadius * w * std::cos(w * simTime + loco.phase));
            } else {
                const float wobble = p.wobble * std::sin(6.2831853f * p.wobbleHz * simTime);
                linear = (h + side * wobble) * loco.speed;
            }
        }
        break;
    }
    case LocomotionType::Pseudopod: {
        loco.speed = 0.0f;
        if (!loco.active)
            return;
        const Vec2 to = {minImage(loco.target.x - c.x, width), minImage(loco.target.y - c.y, height)};
        if (length(to) < 1.0f)
            return;
        const Vec2 u = normalized(to);
        if (loco.testate) {
            // Tecameba: los seudópodos se adhieren adelante de la abertura y tiran; la testa rígida se
            // traslada entera y gira hasta dejar la abertura (el polo anterior) hacia donde va.
            loco.speed = 0.5f * p.pseudopodSpeed * loco.power;
            linear = u * loco.speed;
            omega = std::clamp(std::atan2(cross(h, u), dot(h, u)), -0.5f, 0.5f);
            break;
        }
        // Frente: la membrana orientada hacia el objetivo protruye; cola: se contrae.
        for (std::size_t i = 0; i < body.ring.size(); ++i) {
            const Vec2 o = outwardNormal(body, bp, i);
            const float w = dot(o, u);
            if (w > 0.6f)
                addVelocity(bp, body.ring[i], o * (p.pseudopodSpeed * loco.power * (w - 0.6f) / 0.4f));
            else if (w < -0.2f)
                addVelocity(bp, body.ring[i], o * (p.retractSpeed * loco.power * w));
        }
        for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k)
            addVelocity(bp, body.interior[k], u * p.streamingSpeed);
        return;
    }
    case LocomotionType::None: return; // no se mueve sola
    case LocomotionType::Crawl: {
        // `loco.speed` [µm/s reales] y `loco.target` los fija el mundo (paseo persistente en
        // tiempo biológico). El cuerpo entero se traslada (las adhesiones focales lo llevan) y se
        // alinea con la dirección; el borde de adelante (lamelipodio) protruye un poco más.
        if (!loco.active || loco.speed <= 0.0f)
            return;
        const Vec2 to = {minImage(loco.target.x - c.x, width), minImage(loco.target.y - c.y, height)};
        if (length(to) < 1e-3f)
            return;
        const Vec2 u = normalized(to);
        linear = u * loco.speed;
        // El eje largo del cuerpo (cualquiera de sus dos sentidos) gira hacia la dirección: se
        // realinea en lo que tarda en avanzar medio largo de cuerpo.
        const float delta = std::atan2(cross(h, u), dot(h, u));
        const float axial = delta > 1.5707963f ? delta - 3.1415927f : delta < -1.5707963f ? delta + 3.1415927f : delta;
        omega = axial * loco.speed / std::max(5.0f, halfLength(body));
        for (std::size_t i = 0; i < body.ring.size(); ++i) {
            const float w = dot(outwardNormal(body, bp, i), u);
            if (w > 0.7f)
                addVelocity(bp, body.ring[i], u * (0.5f * loco.speed * (w - 0.7f) / 0.3f));
        }
        break;
    }
    }

    // Cuerpo rígido en el plano: traslación + rotación alrededor del centroide.
    auto drive = [&](std::uint32_t s) {
        const Vec2 r = pos(bp, s) - c;
        addVelocity(bp, s, linear + Vec2{-r.y, r.x} * omega);
    };
    for (std::uint32_t s : body.ring)
        drive(s);
    for (std::uint32_t s : body.interior)
        drive(s);
}

void applySwimmerForces(const RingBody& body, const BodyParticles& bp, const Locomotion& loco,
                        const LocomotionParams& p, Fluid& fluid, bool oralCurrent) {
    const Vec2 c = body.centroid;
    const Vec2 h = bodyHeading(body);
    const float half = halfLength(body);

    // Deslizándose, el flagelo empuja contra el vidrio, no contra el agua: no hay dipolo.
    if (loco.type == LocomotionType::Flagellum && loco.speed > 0.0f && !loco.gliding) {
        // "Puller" (como Chlamydomonas): el flagelo anterior empuja agua hacia atrás y el
        // cuerpo arrastra agua hacia adelante. Fuerzas opuestas, fuerza neta cero.
        // "Pusher" (flagelo posterior: coanoflagelados, espermatozoides): el flagelo empuja agua hacia
        // atrás desde la cola y el cuerpo la arrastra hacia adelante por delante: el par al revés.
        const float r = std::max(4.0f, 0.4f * half);
        const float sign = loco.pusher ? -1.0f : 1.0f;
        pushWater(fluid, c + h * (sign * (half + r)), h * -1.0f, 0.5f * loco.speed, r);
        pushWater(fluid, c - h * (sign * (half + r)), h, 0.5f * loco.speed, r);
    }
    if (oralCurrent && !body.ring.empty()) {
        // Corriente oral: los cilios del surco oral llevan agua (y bacterias) a la boca.
        const std::size_t i = loco.mouthRing % body.ring.size();
        const std::size_t n = body.ring.size();
        const Vec2 m = pos(bp, body.ring[i]);
        const Vec2 t = normalized(pos(bp, body.ring[(i + 1) % n]) - pos(bp, body.ring[(i + n - 1) % n]));
        const Vec2 out = polygon::inwardNormal(t, body.restArea) * -1.0f;
        constexpr float kR = 8.0f;
        pushWater(fluid, m + out * kR, out * -1.0f, p.oralCurrent, kR);
        // Reacción del lado opuesto de la célula: fuerza neta cero.
        pushWater(fluid, c - out * (length(m - c) + kR), out, p.oralCurrent, kR);
    }
}

float maxSpeedOf(const LocomotionParams& p, LocomotionType type) {
    switch (type) {
    case LocomotionType::Cilia: return p.ciliaSpeed;
    case LocomotionType::Flagellum: return p.flagellumSpeed;
    case LocomotionType::Pseudopod: return p.pseudopodSpeed;
    case LocomotionType::Crawl: return 1.0f;
    case LocomotionType::None: return 1.0f;
    }
    return 1.0f;
}
