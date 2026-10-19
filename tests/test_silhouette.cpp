// Prolongaciones en el cuerpo (bio/silhouette): cuernos, brazos y la espina caudal son parte de la
// célula y de su anillo de membrana, unidos al cuerpo con un redondeo, y la pared las mantiene rígidas.
#include "bio/silhouette.hpp"
#include "bio/species.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kPi = 3.14159265f;

bool simpleOutline(const std::vector<Vec2>& p) {
    const std::size_t n = p.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1)
                continue;
            const Vec2 a = p[i], c = p[(i + 1) % n], d = p[j], e = p[(j + 1) % n];
            // Con margen: dos tramos de un mismo borde recto son colineales y el redondeo puede dar
            // productos apenas negativos sin que se crucen.
            constexpr float kEps = 1e-6f;
            if (cross(c - a, d - a) * cross(c - a, e - a) < -kEps && cross(e - d, a - d) * cross(e - d, c - d) < -kEps)
                return false;
        }
    return true;
}

// Giro máximo entre aristas consecutivas en la zona de las uniones (lejos de las puntas, que son
// agudas de verdad): un empalme suave no tiene esquinas.
float maxTurnNear(const std::vector<Vec2>& p, Vec2 center, float radius) {
    float worst = 0.0f;
    const std::size_t n = p.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (length(p[i] - center) > radius)
            continue;
        const Vec2 a = p[(i + n - 1) % n], b = p[i], c = p[(i + 1) % n];
        const Vec2 u = b - a, v = c - b;
        const float lu = length(u), lv = length(v);
        if (lu <= 0.0f || lv <= 0.0f)
            continue;
        worst = std::max(worst, std::acos(std::clamp(dot(u, v) / (lu * lv), -1.0f, 1.0f)));
    }
    return worst;
}

} // namespace

TEST_CASE("silueta: Ceratium es un solo contorno con sus tres cuernos, suave en las uniones") {
    const auto id = findSpecies("ceratium");
    REQUIRE(id);
    const SpeciesInfo& info = speciesInfo(*id);
    REQUIRE(info.outline.size() > 100);
    CHECK(simpleOutline(info.outline));
    CHECK(polygon::signedArea(info.outline.data(), static_cast<int>(info.outline.size())) > 0.0f); // antihorario
    // Más área que el cuerpo solo (los cuernos), pero no el doble: tienen poco citoplasma.
    CHECK(info.outlineAreaFactor > 1.1f);
    CHECK(info.outlineAreaFactor < 2.0f);
    // El cuerno apical llega a su largo desde el polo anterior del cuerpo (90 µm, ±3 por el redondeo y la
    // grilla), y el contorno está centrado en el promedio de sus puntos, como el cuerpo blando.
    float front = -1e9f;
    Vec2 mean;
    for (const Vec2& q : info.outline) {
        front = std::max(front, q.x);
        mean = mean + q;
    }
    mean = mean * (1.0f / static_cast<float>(info.outline.size()));
    const float bodyPole = info.outlineBodyCenter.x + info.spec.radiusX;
    CHECK(front - bodyPole == doctest::Approx(90.0f).epsilon(0.04));
    CHECK(length(mean) < 0.5f);
    // Uniones suaves: alrededor del cuerpo, ningún giro brusco entre aristas de ~0,5 µm.
    CHECK(maxTurnNear(info.outline, info.outlineBodyCenter, info.spec.radiusX + 6.0f) < 25.0f * kPi / 180.0f);
}

TEST_CASE("silueta: las espinas de una cubierta no son parte del cuerpo") {
    for (const char* key : {"trachelomonas", "coleps", "euglypha"}) {
        CAPTURE(std::string(key));
        const auto id = findSpecies(key);
        REQUIRE(id);
        CHECK(speciesInfo(*id).outline.empty());
        CHECK_FALSE(speciesInfo(*id).processes.empty());
    }
    // La espina caudal de Phacus es la película de la célula: del cuerpo.
    const auto phacus = findSpecies("phacus");
    REQUIRE(phacus);
    CHECK_FALSE(speciesInfo(*phacus).outline.empty());
}

TEST_CASE("silueta: la célula nace con su anillo sobre el contorno y centrada como el cuerpo blando") {
    const auto id = findSpecies("staurastrum");
    REQUIRE(id);
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    World world(config);
    const entt::entity e = world.spawnOrganism(*id, {0.5f * world.width(), 0.5f * world.height()}, speciesGenome(*id), 0);
    const RingBody& body = world.registry().get<RingBody>(e);
    Vec2 mean;
    float reach = 0.0f;
    for (const Vec2& q : body.restRing) {
        mean = mean + q;
        reach = std::max(reach, length(q));
    }
    mean = mean * (1.0f / static_cast<float>(body.restRing.size()));
    CHECK(length(mean) < 1e-3f);
    // Los brazos están en el anillo: llega a la punta (13 µm de cuerpo + 34 µm de brazo, ±3).
    CHECK(reach > 40.0f);
    // El núcleo queda en el cuerpo, no en un brazo.
    const CellAnatomy& an = world.registry().get<CellAnatomy>(e);
    const Vec2 nucleus = soft_body::toLocal(body, {world.bodies().x[an.nucleus], world.bodies().y[an.nucleus]});
    CHECK(length(nucleus - speciesInfo(*id).outlineBodyCenter) < speciesInfo(*id).spec.radiusX);
}

TEST_CASE("silueta: la teca es rígida (Ceratium nadando conserva el largo de sus cuernos)" * doctest::test_suite("largos")) {
    const auto id = findSpecies("ceratium");
    REQUIRE(id);
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    World world(config);
    const entt::entity e = world.spawnOrganism(*id, {0.5f * world.width(), 0.5f * world.height()}, speciesGenome(*id), 0);
    auto span = [&] {
        const RingBody& b = world.registry().get<RingBody>(e);
        float lo = 1e9f, hi = -1e9f;
        for (std::uint32_t s : b.ring) {
            const Vec2 q = soft_body::toLocal(b, {world.bodies().x[s], world.bodies().y[s]});
            lo = std::min(lo, q.x);
            hi = std::max(hi, q.x);
        }
        return hi - lo;
    };
    const float before = span();
    for (int s = 0; s < 60 * 20; ++s)
        world.step(kDt);
    CHECK(span() == doctest::Approx(before).epsilon(0.05));
    const RingBody& b = world.registry().get<RingBody>(e);
    std::vector<Vec2> poly(b.poly.begin(), b.poly.end());
    CHECK(simpleOutline(poly));
}

TEST_CASE("silueta: Ceratium se divide en dos hijas con cuernos" * doctest::test_suite("largos")) {
    const auto id = findSpecies("ceratium");
    REQUIRE(id);
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.seed = 52;
    World world(config);
    const entt::entity e = world.spawnOrganism(*id, {0.5f * world.width(), 0.5f * world.height()}, speciesGenome(*id), 0);
    REQUIRE(world.forceReproduction(e));
    int cells = 0;
    for (int s = 0; s < 60 * 180 && cells < 2; ++s) {
        world.step(kDt);
        cells = static_cast<int>(world.registry().view<Species>().size());
    }
    REQUIRE(cells == 2);
    for (int s = 0; s < 60 * 20; ++s)
        world.step(kDt);
    for (auto [c, sp, b] : world.registry().view<Species, RingBody>().each()) {
        std::vector<Vec2> poly(b.poly.begin(), b.poly.end());
        CHECK(simpleOutline(poly));
        // Cada hija tiene la forma de su especie: su alcance es el de la silueta a su tamaño.
        float reach = 0.0f;
        for (const Vec2& q : b.restRing)
            reach = std::max(reach, length(q));
        const float k = std::sqrt(std::abs(b.restArea) / speciesInfo(*id).outlineArea);
        CHECK(reach == doctest::Approx(speciesInfo(*id).outlineReach * k).epsilon(0.1));
    }
}
