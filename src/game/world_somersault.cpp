// Volteretas de Closterium: la desmidia secreta mucílago por los poros de una punta, que queda pegada al
// vidrio; la otra se levanta, la célula se para sobre la punta fija y cae del otro lado, y la vez
// siguiente apoya la otra punta: avanza de a un largo por voltereta (Yeh & Gibor 1970, Planta 93: 316).
// Vista desde arriba: se acorta hacia la punta fija hasta verse casi de punta (girar alrededor de un eje
// del plano de la vista acorta la proyección) y se vuelve a alargar del otro lado. La media luna es
// simétrica, así que caída del otro lado es la misma forma corrida un largo: la plantilla pasa a
// anclarse en la otra punta y el anillo no se invierte.
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "game/rest_shape.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Parada, la célula se ve de punta: un corto del largo de su espesor, no un punto.
constexpr float kEndOn = 0.15f;
// Tasa con que la punta fija se mantiene en su lugar (como el pie de Stentor).
constexpr float kHoldRate = 6.0f; // [1/s]
} // namespace

void World::updateSomersault(entt::entity e, RingBody& body, Locomotion& loco, float dt) {
    PROFILE_SCOPE("World::updateSomersault");
    const SpeciesInfo& info = speciesOfCell(e);
    Somersault& so = registry_.get_or_emplace<Somersault>(e);
    if (registry_.any_of<Dividing, Cyst>(e)) {
        stopSomersault(e, body);
        return;
    }
    const std::size_t n = body.restRing.size();
    if (so.t < 0.0f) {
        if (so.wait < 0.0f) { // espera exponencial hasta la próxima
            Rng rng(config_.seed, rng_stream::kSomersault, body.id, step_);
            so.wait = -info.somersaultInterval * std::log(1.0f - 0.999f * rng.uniform());
        }
        so.wait -= dt;
        if (so.wait > 0.0f)
            return;
        // Empieza: la plantilla es la forma de ahora; las puntas, los extremos del eje.
        so.base = body.restRing;
        std::size_t lo = 0, hi = 0;
        for (std::size_t i = 1; i < n; ++i) {
            if (so.base[i].x < so.base[lo].x)
                lo = i;
            if (so.base[i].x > so.base[hi].x)
                hi = i;
        }
        so.pivot = so.frontNext ? hi : lo;
        so.other = so.frontNext ? lo : hi;
        so.frontNext = !so.frontNext;
        so.axis = bodyHeading(body);
        const std::uint32_t p = body.ring[so.pivot];
        so.hold = {bodies_.x[p], bodies_.y[p]};
        so.t = 0.0f;
        so.areaScale = 1.0f;
    }
    if (so.base.size() != n) {
        stopSomersault(e, body);
        return;
    }
    so.t = std::min(1.0f, so.t + dt / info.somersaultSeconds);
    const float theta = units::kPi * so.t;
    const bool rising = so.t < 0.5f;
    const float c = std::max(std::abs(std::cos(theta)), kEndOn);
    const float xp = so.base[so.pivot].x, xo = so.base[so.other].x;
    const float span = std::abs(xo - xp);
    const float dir = xo > xp ? 1.0f : -1.0f;
    // Corrimiento para que, parada, la célula quede centrada en la punta fija (las dos fases coinciden).
    const float w = (1.0f - (c - kEndOn) / (1.0f - kEndOn)) * 0.5f * kEndOn * span;
    thread_local std::vector<Vec2> shape;
    shape = so.base;
    for (Vec2& q : shape)
        q.x = rising ? xp + (q.x - xp) * c - dir * w : xp + (q.x - xo) * c + dir * w;
    const float area = std::abs(polygon::signedArea(shape.data(), static_cast<int>(n)));
    const float area0 = std::abs(polygon::signedArea(so.base.data(), static_cast<int>(n)));
    const float scale = area0 > 0.0f ? std::max(0.05f, area / area0) : 1.0f;
    rest_shape::apply(body, shape, scale, so.areaScale);
    so.areaScale = scale;
    // La punta apoyada (la fija mientras sube; la otra, que cae sobre el mismo lugar, después) se queda
    // donde está; el resto de la célula se mueve alrededor.
    const std::size_t held = rising ? so.pivot : so.other;
    const float offset = rising ? -dir * w : dir * w;
    const Vec2 target = so.hold + so.axis * offset;
    const std::uint32_t hs = body.ring[held];
    const Vec2 pull = (target - Vec2{bodies_.x[hs], bodies_.y[hs]}) * kHoldRate;
    for (std::uint32_t s : body.ring) {
        bodies_.vx[s] += pull.x;
        bodies_.vy[s] += pull.y;
    }
    for (std::uint32_t s : body.interior) {
        bodies_.vx[s] += pull.x;
        bodies_.vy[s] += pull.y;
    }
    loco.forward = loco.turn = 0.0f; // durante la voltereta no se desliza
    loco.active = false;
    if (so.t >= 1.0f) { // ya acostada del otro lado: vuelve a la forma entera
        rest_shape::apply(body, so.base, 1.0f, so.areaScale);
        so.areaScale = 1.0f;
        so.t = -1.0f;
        so.wait = -1.0f;
        so.base.clear();
    }
}

void World::stopSomersault(entt::entity e, RingBody& body) {
    Somersault* so = registry_.try_get<Somersault>(e);
    if (!so || so->t < 0.0f)
        return;
    if (so->base.size() == body.restRing.size())
        rest_shape::apply(body, so->base, 1.0f, so->areaScale);
    so->areaScale = 1.0f;
    so->t = -1.0f;
    so->wait = -1.0f;
    so->base.clear();
}
