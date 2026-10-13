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
    case LocomotionType::Undulation: return "ondulacion";
    }
    return "";
}

Vec2 bodyHeading(const RingBody& body) { return {std::cos(body.angle), std::sin(body.angle)}; }

RigidMotion applyLocomotion(const RingBody& body, BodyParticles& bp, Locomotion& loco,
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
        linear = h * loco.speed;
        omega = loco.turn * p.ciliaTurnRate;
        break;
    }
    case LocomotionType::Flagellum: {
        loco.speed = loco.active ? p.flagellumSpeed * loco.power : 0.0f;
        if (loco.active) {
            const Vec2 to = {minImage(loco.target.x - c.x, width), minImage(loco.target.y - c.y, height)};
            const Vec2 u = normalized(to);
            const float delta = std::atan2(cross(h, u), dot(h, u));
            omega = std::clamp(3.0f * delta, -p.flagellumTurnRate, p.flagellumTurnRate);
            const float wobble = p.wobble * std::sin(6.2831853f * p.wobbleHz * simTime);
            linear = (h + side * wobble) * loco.speed;
        }
        break;
    }
    case LocomotionType::Pseudopod: {
        loco.speed = 0.0f;
        if (!loco.active)
            return {};
        const Vec2 to = {minImage(loco.target.x - c.x, width), minImage(loco.target.y - c.y, height)};
        if (length(to) < 1.0f)
            return {};
        const Vec2 u = normalized(to);
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
        return {};
    }
    case LocomotionType::Undulation: return {}; // ver undulate()
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
    return {linear, omega, c};
}

void applySwimmerForces(const RingBody& body, const BodyParticles& bp, const Locomotion& loco,
                        const LocomotionParams& p, Fluid& fluid, bool oralCurrent) {
    const Vec2 c = body.centroid;
    const Vec2 h = bodyHeading(body);
    const float half = halfLength(body);

    if (loco.type == LocomotionType::Flagellum && loco.speed > 0.0f) {
        // "Puller" (como Chlamydomonas): el flagelo anterior empuja agua hacia atrás y el
        // cuerpo arrastra agua hacia adelante. Fuerzas opuestas, fuerza neta cero.
        const float r = std::max(4.0f, 0.4f * half);
        pushWater(fluid, c + h * (half + r), h * -1.0f, 0.5f * loco.speed, r);
        pushWater(fluid, c - h * (half + r), h, 0.5f * loco.speed, r);
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

// ---------------------------------------------------------------------------
// Nematodo: ondulación y teoría de fuerzas resistivas (Gray y Hancock, 1955)
// ---------------------------------------------------------------------------

namespace {

constexpr int kSpine = 32;

struct Spine {
    Vec2 p[kSpine]; // eje centrado, en el marco del cuerpo (cabeza hacia +x)
    Vec2 t[kSpine]; // tangentes
};

// Eje para una fase: ángulo θ(s) = A·sin(k·s + fase) + giro·s/L, sin el ángulo medio (el marco
// del cuerpo no rota con la onda), integrado y centrado.
void spineShape(float length, float k, float amp, float bend, float phase, Spine& out) {
    float theta[kSpine];
    float mean = 0.0f;
    const float ds = length / static_cast<float>(kSpine - 1);
    for (int j = 0; j < kSpine; ++j) {
        const float s = -0.5f * length + ds * static_cast<float>(j);
        theta[j] = amp * std::sin(k * s + phase) + bend * s / length;
        mean += theta[j];
    }
    mean /= kSpine;
    Vec2 p, sum;
    for (int j = 0; j < kSpine; ++j) {
        theta[j] -= mean;
        if (j > 0) {
            const float a = 0.5f * (theta[j] + theta[j - 1]);
            p = p + Vec2{std::cos(a), std::sin(a)} * ds;
        }
        out.p[j] = p;
        out.t[j] = {std::cos(theta[j]), std::sin(theta[j])};
        sum = sum + p;
    }
    sum = sum * (1.0f / kSpine);
    for (Vec2& q : out.p)
        q = q - sum;
}

// Punto del eje y su tangente en la coordenada s ∈ [−L/2, L/2] (interpolación lineal).
void spineAt(const Spine& sp, float length, float s, Vec2& p, Vec2& t) {
    const float u = std::clamp((s + 0.5f * length) / length, 0.0f, 1.0f) * static_cast<float>(kSpine - 1);
    const int j = std::min(kSpine - 2, static_cast<int>(u));
    const float f = u - static_cast<float>(j);
    p = sp.p[j] + (sp.p[j + 1] - sp.p[j]) * f;
    t = normalized(sp.t[j] + (sp.t[j + 1] - sp.t[j]) * f);
}

// Arrastre local de un tramo: f = −(ξt (v·t) t + ξn (v·n) n), con ξt = 1.
Vec2 drag(Vec2 v, Vec2 t, float ratio) {
    const Vec2 n = {-t.y, t.x};
    return (t * dot(v, t) + n * (ratio * dot(v, n))) * -1.0f;
}

float det3(const float m[3][3]) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

} // namespace

void initUndulation(const RingBody& body, Undulation& und) {
    und.ringS.clear();
    und.ringOffset.clear();
    und.interiorS.clear();
    und.interiorOffset.clear();
    float half = 0.0f;
    for (const Vec2& q : body.restRing) {
        und.ringS.push_back(q.x);
        und.ringOffset.push_back(q.y);
        half = std::max(half, std::abs(q.x));
    }
    for (const Vec2& q : body.restInterior) {
        und.interiorS.push_back(q.x);
        und.interiorOffset.push_back(q.y);
    }
    und.length = 2.0f * half;
}

RigidMotion undulate(RingBody& body, BodyParticles& bp, Locomotion& loco, Undulation& und, const LocomotionParams& p,
                     float crowding, float dt) {
    const float L = std::max(1.0f, und.length);
    // Marcha: entre partículas la onda se acorta y se hace más lenta (adaptación de la marcha
    // a la resistencia del medio, como en C. elegans entre agua y agar).
    und.crowding += (crowding - und.crowding) * std::min(1.0f, 2.0f * dt);
    const float c = und.crowding;
    const float hz = p.swimHz + (p.crawlHz - p.swimHz) * c;
    const float k = 6.2831853f / (L * (p.swimWavelength + (p.crawlWavelength - p.swimWavelength) * c));
    // Sin orden de avanzar los músculos se relajan y el cuerpo se estira (una S congelada a
    // máxima amplitud no es una postura de reposo).
    const float want = std::min(1.0f, std::max(std::abs(loco.forward), 0.0f));
    und.effort += (want - und.effort) * std::min(1.0f, 4.0f * dt);
    const float amp = (p.swimAmplitude + (p.crawlAmplitude - p.swimAmplitude) * c) * und.effort;
    und.dragRatio = p.dragRatioWater + (p.dragRatioSubstrate - p.dragRatioWater) * c;
    // La onda viaja de la cabeza a la cola (fase creciente) para avanzar; al revés, retrocede.
    // Más músculo (gen motor) late más rápido.
    const float omega = 6.2831853f * hz * loco.power * std::clamp(loco.forward, -1.0f, 1.0f);
    und.phase += omega * dt;
    // Mismo convenio que los ciliados: turn > 0 hace crecer el ángulo del rumbo.
    const float bend = -p.undulationTurn * std::clamp(loco.turn, -1.0f, 1.0f);

    Spine sp, ahead, behind;
    spineShape(L, k, amp, bend, und.phase, sp);
    spineShape(L, k, amp, bend, und.phase + 0.01f, ahead);
    spineShape(L, k, amp, bend, und.phase - 0.01f, behind);

    // Forma de reposo: cada punto de la membrana queda a su desvío del eje doblado.
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n && i < und.ringS.size(); ++i) {
        Vec2 q, t;
        spineAt(sp, L, und.ringS[i], q, t);
        body.restRing[i] = q + Vec2{-t.y, t.x} * und.ringOffset[i];
    }
    for (std::size_t i = 0; i < n; ++i)
        body.restEdge[i] = length(body.restRing[(i + 1) % n] - body.restRing[i]);
    for (std::size_t k2 = 0; k2 < body.interior.size() && k2 < und.interiorS.size(); ++k2) {
        Vec2 q, t;
        spineAt(sp, L, std::clamp(und.interiorS[k2], -0.5f * L, 0.5f * L), q, t);
        body.restInterior[k2] = q + Vec2{-t.y, t.x} * und.interiorOffset[k2];
    }

    // Fuerzas resistivas: la velocidad de cada tramo es la de la deformación más el movimiento
    // rígido (U, Ω); sin inercia, la fuerza y el torque totales del agua sobre el cuerpo son
    // nulos. Es un sistema lineal 3×3 en (Ux, Uy, Ω).
    const float ratio = und.dragRatio;
    Vec2 u[kSpine];
    for (int j = 0; j < kSpine; ++j)
        u[j] = (ahead.p[j] - behind.p[j]) * (omega / 0.02f);
    float m[3][3] = {}, rhs[3] = {};
    for (int j = 0; j < kSpine; ++j) {
        const Vec2 r = sp.p[j], t = sp.t[j];
        const Vec2 basis[3] = {{1.0f, 0.0f}, {0.0f, 1.0f}, {-r.y, r.x}};
        for (int col = 0; col < 3; ++col) {
            const Vec2 f = drag(basis[col], t, ratio);
            m[0][col] += f.x;
            m[1][col] += f.y;
            m[2][col] += cross(r, f);
        }
        const Vec2 f0 = drag(u[j], t, ratio);
        rhs[0] -= f0.x;
        rhs[1] -= f0.y;
        rhs[2] -= cross(r, f0);
    }
    const float d = det3(m);
    float x[3] = {};
    if (std::abs(d) > 1e-12f) {
        for (int col = 0; col < 3; ++col) { // regla de Cramer
            float mc[3][3];
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    mc[a][b] = b == col ? rhs[a] : m[a][b];
            x[col] = det3(mc) / d;
        }
    }
    const Vec2 U = {x[0], x[1]};
    const float Omega = x[2];

    // Al mundo: el marco del cuerpo es el del ajuste de la membrana (centroide y ángulo).
    const float ca = std::cos(body.angle), sa = std::sin(body.angle);
    const Vec2 linear = rotate(U, ca, sa);
    const Vec2 center = body.centroid;
    auto drive = [&](std::uint32_t s) {
        const Vec2 r = pos(bp, s) - center;
        addVelocity(bp, s, linear + Vec2{-r.y, r.x} * Omega);
    };
    for (std::uint32_t s : body.ring)
        drive(s);
    for (std::uint32_t s : body.interior)
        drive(s);
    loco.speed = dot(linear, bodyHeading(body)) >= 0.0f ? length(linear) : -length(linear);

    // Eje en el mundo (render) y fuerzas sobre el agua: la reacción del arrastre de cada tramo.
    und.spine.resize(kSpine);
    und.waterAt.resize(kSpine);
    und.waterPush.resize(kSpine);
    for (int j = 0; j < kSpine; ++j) {
        const Vec2 r = sp.p[j];
        und.spine[j] = center + rotate(r, ca, sa);
        const Vec2 v = U + Vec2{-r.y, r.x} * Omega + u[j];
        und.waterAt[j] = und.spine[j];
        und.waterPush[j] = rotate(drag(v, sp.t[j], ratio) * -1.0f, ca, sa);
    }
    return {linear, Omega, center};
}

void applyUndulationForces(const Undulation& und, Fluid& fluid) {
    // Cada tramo arrastra el agua de alrededor con la fuerza que el agua le hace a él (cambiada
    // de signo); como el nado es libre de fuerza, la suma es cero. El agua cerca de un tramo se
    // mueve a una fracción de su velocidad relativa (ξ medio = (1 + ξn/ξt) / 2).
    const float mean = 0.5f * (1.0f + und.dragRatio);
    for (std::size_t j = 0; j < und.waterAt.size(); ++j) {
        const float f = length(und.waterPush[j]);
        if (f > 1e-4f)
            pushWater(fluid, und.waterAt[j], und.waterPush[j] * (1.0f / f), 0.3f * f / mean, 8.0f);
    }
}
