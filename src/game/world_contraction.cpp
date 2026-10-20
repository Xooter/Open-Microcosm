// Contracción del cuerpo entero (Stentor, Spirostomum): mecanismo de docs/plan_mecanicas.md (punto 8).
#include "core/profiling.hpp"
#include "game/world.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Las células con contracción siempre tienen especie (como en game/world_animal.cpp).
const SpeciesInfo& infoOf(const entt::registry& reg, entt::entity e) {
    static const SpeciesInfo kNone;
    const Species* sp = reg.try_get<Species>(e);
    return sp ? speciesInfo(sp->id) : kNone;
}
// La contracción real dura ~5–10 ms, menos que un paso: la forma de reposo salta a la contraída y la
// memoria de forma muy alta lleva ahí al cuerpo en unos pocos pasos.
constexpr float kContractRate = 40.0f; // [1/s]
} // namespace

namespace {
// Estira la forma de reposo a lo largo de su eje (x local) de `from` a `to` veces la contraída, con el
// ancho en proporción inversa (el área la fija la biomasa). Afín: cada punto de la membrana sigue siendo
// el mismo (rehacer el reparto por longitud de arco en cada paso corre los puntos por el contorno y la
// membrana "rueda": una célula anclada se arrastraría).
void stretchRest(RingBody& body, float from, float to) {
    const float k = to / from;
    for (Vec2& q : body.restRing) {
        q.x *= k;
        q.y /= k;
    }
}
float restHalfLength(const RingBody& body) {
    float l = 0.0f;
    for (const Vec2& q : body.restRing)
        l = std::max(l, std::abs(q.x));
    return l;
}
} // namespace

void World::updateContraction(entt::entity e, RingBody& body, bool trigger, float dt) {
    PROFILE_SCOPE("World::updateContraction");
    const SpeciesInfo& info = infoOf(registry_, e);
    const ContractionSpec& spec = info.contraction;
    if (spec.extension <= 1.0f)
        return;
    Contraction& c = registry_.get_or_emplace<Contraction>(e);
    // Dividiéndose o en quiste, la forma la llevan la división o la pared (desde la contraída).
    if (registry_.any_of<Dividing, Cyst>(e)) {
        c.state = 1.0f;
        c.stretch = 1.0f;
        return;
    }
    const float before = c.state;
    if (trigger)
        c.state = 1.0f;
    else
        c.state = std::max(0.0f, c.state - dt / spec.extensionSeconds);
    if (!trigger && c.state == before)
        return; // quieta, extendida del todo
    // Re-extensión suave; la contracción, de golpe.
    const float t = 1.0f - c.state;
    const float stretch = 1.0f + (spec.extension - 1.0f) * t * t * (3.0f - 2.0f * t);
    if (trigger && c.stretch > 1.05f) {
        // Los dos extremos se acercan al centro de golpe y arrastran el agua.
        const float half = restHalfLength(body);
        const float shortening = half * (1.0f - stretch / c.stretch);
        contractionKicks_.push_back({body.centroid, bodyHeading(body), half, shortening / dt,
                                     std::max(6.0f, 0.5f * std::abs(body.restArea) / (units::kPi * half))});
    }
    stretchRest(body, c.stretch, stretch);
    c.stretch = stretch;
    const CellShape* shape = registry_.try_get<CellShape>(e);
    body.shapeRate = c.state > 0.9f ? kContractRate : shape ? shape->shapeRate : body.shapeRate;
}

void World::contractForDivision(entt::entity e, RingBody& body) {
    // Simplificación: la célula se divide con la forma contraída de la especie (el surco de la división
    // no llega a cerrar en una célula de casi 1 mm). Stentor y Spirostomum reales se dividen extendidos.
    Contraction* c = registry_.try_get<Contraction>(e);
    if (!c || c->stretch <= 1.0f)
        return;
    stretchRest(body, c->stretch, 1.0f);
    c->state = 1.0f;
    c->stretch = 1.0f;
}

void World::applyContractionKicks() {
    // Fuerza que mueve el agua a ~`speed` en cada extremo, hacia el centro (par de fuerzas opuestas:
    // fuerza neta cero, como todo nadador a bajo Reynolds).
    for (const ContractionKick& kick : contractionKicks_) {
        const float response = fluid_.viscosity / (kick.radius * kick.radius) + fluid_.brinkmanFriction();
        for (float side : {1.0f, -1.0f}) {
            const Vec2 pole = kick.center + kick.axis * (side * kick.halfLength);
            fluid_.addForce(pole, kick.axis * (-side * kick.speed * response), kick.radius);
        }
    }
    contractionKicks_.clear();
}
