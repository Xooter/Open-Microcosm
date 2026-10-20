// Cuello de Lacrymaria olor: no es un apéndice aparte sino la parte anterior de la célula, un tubo de
// citoplasma reforzado por microtúbulos que se estira hasta ~7 veces el largo del cuerpo en segundos y se
// retrae (Coyle, Flaum, Li, Krishnamurthy & Prakash 2019, Curr. Biol. 29: R1192: lo hace una corteza
// de bandas helicoidales que se despliega). Acá un tramo de puntos de membrana del polo anterior se
// despliega en un tubo del largo del cuello, hacia la presa.
#include "core/profiling.hpp"
#include "game/rest_shape.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kNeckHalfWidth = 2.0f;   // [µm] (cuello de ~4 µm de diámetro)
constexpr float kNeckMaxAngle = 1.3f;    // [rad] respecto del eje del cuerpo
constexpr float kNeckBlend = 10.0f;      // [µm] de largo en que el polo pasa a ser tubo

std::size_t neckIndex(const Neck& neck, std::size_t i, std::size_t n) { return (neck.first + i) % n; }
} // namespace

void World::updateNeckShape(entt::entity e, RingBody& body) {
    PROFILE_SCOPE("World::updateNeckShape");
    Neck* neck = registry_.try_get<Neck>(e);
    const std::size_t n = body.restRing.size();
    if (!neck || n < 2 * kNeckPoints || registry_.any_of<Dividing, Cyst>(e))
        return;
    if (neck->base.size() != n) {
        if (neck->length > 1.0f)
            return; // se arma con el cuello retraído (la forma de reposo es la del cuerpo)
        neck->base = body.restRing;
        std::size_t front = 0;
        for (std::size_t i = 1; i < n; ++i)
            if (neck->base[i].x > neck->base[front].x)
                front = i;
        neck->first = (front + n - kNeckPoints / 2) % n;
        neck->areaScale = 1.0f;
    }
    // Dirección del cuello en el marco de la célula, acotada hacia adelante.
    const float co = std::cos(body.angle), si = std::sin(body.angle);
    Vec2 d = {neck->direction.x * co + neck->direction.y * si, -neck->direction.x * si + neck->direction.y * co};
    float a = std::atan2(d.y, d.x);
    a = std::clamp(a, -kNeckMaxAngle, kNeckMaxAngle);
    d = {std::cos(a), std::sin(a)};
    Vec2 side = {-d.y, d.x};
    if (body.restArea < 0.0f) // anillo horario: los lados al revés
        side = side * -1.0f;
    const std::size_t front = neckIndex(*neck, kNeckPoints / 2, n);
    const Vec2 root = neck->base[front];
    const float blend = std::clamp(neck->length / kNeckBlend, 0.0f, 1.0f);
    const std::size_t half = kNeckPoints / 2;
    thread_local std::vector<Vec2> shape;
    shape = neck->base;
    for (std::size_t i = 0; i < kNeckPoints; ++i) {
        // La primera mitad sale por un lado y la segunda vuelve por el otro (en el orden del anillo).
        const bool out = i < half;
        const float t = out ? static_cast<float>(i) / static_cast<float>(half - 1)
                            : static_cast<float>(kNeckPoints - 1 - i) / static_cast<float>(half - 1);
        const Vec2 tube = root + d * (neck->length * t) + side * (out ? -kNeckHalfWidth : kNeckHalfWidth);
        Vec2& q = shape[neckIndex(*neck, i, n)];
        q = q + (tube - q) * blend;
    }
    // El cuello agrega silueta (el cuerpo no se achica por eso: en volumen el tubo fino es poco).
    const float area = std::abs(polygon::signedArea(shape.data(), static_cast<int>(n)));
    const float area0 = std::abs(polygon::signedArea(neck->base.data(), static_cast<int>(n)));
    const float scale = area0 > 0.0f ? area / area0 : 1.0f;
    rest_shape::apply(body, shape, scale, neck->areaScale);
    neck->areaScale = scale;
    if (neck->length <= 0.0f)
        neck->base.clear(); // retraído: la próxima vez se toma la forma de entonces (creció, se dividió)
}

void World::retractNeck(entt::entity e, RingBody& body) {
    Neck* neck = registry_.try_get<Neck>(e);
    if (!neck)
        return;
    if (neck->base.size() == body.restRing.size()) {
        rest_shape::apply(body, neck->base, 1.0f, neck->areaScale);
        neck->areaScale = 1.0f;
    }
    neck->length = 0.0f;
    neck->base.clear();
}

Vec2 World::neckBase(entt::entity e, const RingBody& body) const {
    const Neck* neck = registry_.try_get<Neck>(e);
    const std::size_t n = body.ring.size();
    if (neck && neck->base.size() == n) { // entre los dos extremos del tramo del cuello
        const std::uint32_t a = body.ring[neckIndex(*neck, 0, n)], b = body.ring[neckIndex(*neck, kNeckPoints - 1, n)];
        return Vec2{bodies_.x[a] + bodies_.x[b], bodies_.y[a] + bodies_.y[b]} * 0.5f;
    }
    const std::uint32_t front = body.ring[poleIndex(body, true)];
    return {bodies_.x[front], bodies_.y[front]};
}

bool World::neckTip(entt::entity e, const RingBody& body, Vec2& tip) const {
    const Neck* neck = registry_.try_get<Neck>(e);
    const std::size_t n = body.ring.size();
    if (!neck || neck->base.size() != n || neck->length < 1.0f)
        return false;
    const std::uint32_t a = body.ring[neckIndex(*neck, kNeckPoints / 2 - 1, n)];
    const std::uint32_t b = body.ring[neckIndex(*neck, kNeckPoints / 2, n)];
    tip = Vec2{bodies_.x[a] + bodies_.x[b], bodies_.y[a] + bodies_.y[b]} * 0.5f;
    return true;
}
